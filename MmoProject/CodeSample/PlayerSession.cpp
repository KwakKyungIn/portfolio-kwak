#include "pch.h"
#include "PlayerSession.h"
#include "ClientPacketHandler.h"
#include "GameSessionManager.h"
#include "GameRoom.h"
#include "Player.h"
#include "InstanceActor.h"
#include "RoomManager.h"
#include "AutoCommitService.h"
#include "PersistenceService.h"
#include "GameMetrics.h"

// 클라가 접속하면 호출됨
void PlayerSession::OnConnected()
{
    ASSERT_CRASH(GameSessionManager::GSessionManager != nullptr);

    // 전체 세션 관리자에 내 세션을 등록함
    // 이제부터 서버가 내 존재를 알게 됨
    GameSessionManager::GSessionManager->Add(static_pointer_cast<PlayerSession>(shared_from_this()));
}

// 접속 끊기면 호출됨 (여기가 제일 복잡함)
void PlayerSession::OnDisconnected()
{
    auto self = static_pointer_cast<PlayerSession>(shared_from_this());

    // 관리자 목록에서 즉시 제거
    GameSessionManager::GSessionManager->Remove(self);

    // 정리 작업은 Session Actor 스레드(JobQueue)에 넣어서 순차적으로 처리
    // 동시성 문제 안 생기게 하려고 Post 사용함
    Post([=](PlayerSessionRef ps)
        {
            // 맵 이동 중이었다면 취소
            ps->CancelMapChange();

            // 세션은 Player 객체를 직접 들고 있지 않고 ID만 들고 있음 (순환참조 방지)
            const uint64 playerId = ps->GetPlayerId_AnyThread();

            // 더 이상 패킷을 못 받게 현재 룸과의 연결을 끊음
            RoomActorRef room = ps->GetCurrentRoom_ActorOnly();
            ps->SetCurrentRoom(nullptr);

            // 로그인 상태였다면 정리 작업 수행
            if (playerId != 0)
            {
                GameMetrics::OnLobbyEnterCancelled(playerId);

                // [중요] DB 저장 트리거 (PersistenceService)
                // 연결 끊기면 무조건 Dirty 찍어서 저장하게 만듦
                // 나중에 최적화해서 진짜 변한 게 있을 때만 찍도록 수정할 예정
                Persistence::PersistenceService::I().MarkDirty_PlayerCore(playerId);
                Persistence::PersistenceService::I().MarkDirty_Inventory(playerId);
                Persistence::AutoCommitService::I().RequestFlushNow(playerId);

                // 세션 매니저에서도 ID 매핑 해제
                GameSessionManager::GSessionManager->UnbindPlayerId(playerId);
                ps->ClearPlayerId_ActorOnly();
            }

            // 플레이어가 게임 룸에 있었다면 퇴장 처리
            // 이건 룸 스레드에서 처리해야 하므로 PushJob으로 넘김
            if (playerId != 0 && room && room->GetKind() == RoomKind::Game)
            {
                auto gr = std::dynamic_pointer_cast<GameRoom>(room);
                if (gr)
                {
                    // "나 나간다"고 룸에 알림
                    gr->PushJob(&GameRoom::LeaveById, ps, playerId);
                }
            }

            // 만약 인스턴스 던전 같은 곳에 있었다면?
            // 혼자 있는 방이면 방을 폭파시켜야 함
            if (playerId != 0)
            {
                InstanceActor::Instance().Push([playerId]()
                    {
                        InstanceManagerCore::InstanceInfo closed;
                        // 멤버 오프라인 처리하고 방 닫아야 하는지 확인
                        if (InstanceActor::Instance().Core().OnMemberOffline(playerId, closed))
                        {
                            if (closed.instanceId != 0 && GRoomManager)
                            {
                                auto r = GRoomManager->FindRoom(closed.channelId, closed.mapId, closed.instanceId);
                                if (r) r->MarkClosing(true); // 방 닫음
                            }
                        }
                    });
            }
        });
}

// 패킷 들어오면 호출됨
void PlayerSession::OnRecvPacket(BYTE* buffer, int32 len)
{
    PacketSessionRef session = GetPacketSessionRef();
    // 패킷 핸들러로 토스. 거기서 ID 보고 분기 처리함
    ClientPacketHandler::HandlePacket(session, buffer, len);
}

void PlayerSession::OnSend(int32 len)
{
    // 전송 완료 콜백. 지금은 딱히 할 게 없음
}

// 하트비트 체크
void PlayerSession::Ping()
{
    Protocol::S_HEART_BEAT_RES pkt;
    auto sendBuffer = ClientPacketHandler::MakeSendBuffer(pkt);
    Send(sendBuffer);
    std::cout << "[Server] Ping -> Client" << std::endl;
}

// 맵 이동 시작 요청 (Try 패턴)
// 멀티스레드 환경이라 락 걸고 상태 확인해야 함
bool PlayerSession::TryBeginMapChange(uint64 token, int32 targetChannelId, int32 targetMapId, int64 targetInstanceId, const Protocol::PositionInfo& spawn)
{
    std::lock_guard<std::mutex> lock(_mapChangeLock);

    // 이미 이동 중이면 거절
    if (_mapChangeState.load(std::memory_order_relaxed) != MAP_CHANGE_NONE)
        return false;

    // 이동할 목적지 정보 저장
    _mapChangeToken = token;
    _pendingTargetChannelId = targetChannelId;
    _pendingTargetMapId = targetMapId;
    _pendingTargetInstanceId = targetInstanceId;
    _pendingSpawn.CopyFrom(spawn);

    // 상태를 'ACK 대기 중'으로 변경
    _mapChangeState.store(MAP_CHANGE_WAITING_ACK, std::memory_order_release);
    return true;
}

// 맵 이동 상태 초기화 (내부용)
void PlayerSession::ResetMapChangeState_Locked()
{
    _mapChangeToken = 0;
    _pendingTargetChannelId = 0;
    _pendingTargetMapId = 0;
    _pendingTargetInstanceId = 0;
    _pendingSpawn.Clear();

    _mapChangeState.store(MAP_CHANGE_NONE, std::memory_order_release);
}

// 클라에서 "이동 준비 됐어요"라고 응답 오면 호출
bool PlayerSession::TryConsumeMapChangeAck(uint64 token, int32& outTargetChannelId, int32& outTargetMapId, int64& outTargetInstanceId, Protocol::PositionInfo& outSpawn)
{
    std::lock_guard<std::mutex> lock(_mapChangeLock);

    // 상태 체크: 대기 중이어야 하고 토큰이 맞아야 함
    if (_mapChangeState.load(std::memory_order_relaxed) != MAP_CHANGE_WAITING_ACK)
        return false;
    if (_mapChangeToken != token)
        return false;

    // 대기 중이던 목적지 정보를 밖으로 빼줌
    outTargetChannelId = _pendingTargetChannelId;
    outTargetMapId = _pendingTargetMapId;
    outTargetInstanceId = _pendingTargetInstanceId;
    outSpawn.CopyFrom(_pendingSpawn);

    // 상태를 '교체 중'으로 변경
    _mapChangeState.store(MAP_CHANGE_SWITCHING, std::memory_order_release);
    return true;
}

// 맵 이동 끝났을 때
void PlayerSession::EndMapChange()
{
    std::lock_guard<std::mutex> lock(_mapChangeLock);
    ResetMapChangeState_Locked();
}

// 맵 이동 취소 (에러 났을 때 등)
void PlayerSession::CancelMapChange()
{
    std::lock_guard<std::mutex> lock(_mapChangeLock);
    ResetMapChangeState_Locked();
}

uint64 PlayerSession::GetMapChangeToken() const
{
    std::lock_guard<std::mutex> lock(_mapChangeLock);
    return _mapChangeToken;
}

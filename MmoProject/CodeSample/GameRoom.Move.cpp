#include "pch.h"
#include "GameRoom.h"
#include "GameMap.h"
#include "Player.h"
#include "PlayerSession.h"
#include "Monster.h"
#include "ExperimentUtils.h"
#include "GameMetrics.h"
#include "RoomManager.h"
#include "GameRoom.Net.h"
#include "MoveValidationUtils.h"
#include <algorithm>
#include <cmath>
#include <iostream> 
#include <iomanip>  

// 좌표값이 유효한 숫자인지 체크 (NaN, Inf 방지)
static bool IsFinitePos(const Protocol::PositionInfo& p)
{
    return std::isfinite(p.x()) && std::isfinite(p.y()) &&
        std::isfinite(p.z()) && std::isfinite(p.yaw());
}

// 좌표값이 터무니없이 큰지 체크 (핵 방지 및 버그 방지)
static bool IsCrazyPos(const Protocol::PositionInfo& p)
{
    constexpr float kMaxAbs = 100000.0f; // 맵 밖으로 튕겨나가는 거 방지용 리미트
    return (std::fabs(p.x()) > kMaxAbs) || (std::fabs(p.y()) > kMaxAbs) || (std::fabs(p.z()) > kMaxAbs);
}

static float GetYawDelta(float lhs, float rhs)
{
    float delta = std::fabs(lhs - rhs);
    while (delta > 360.f)
        delta -= 360.f;

    const float wrappedDelta = 360.f - delta;
    return (delta < wrappedDelta) ? delta : wrappedDelta;
}

static bool NeedsOwnerCorrection(const Protocol::PositionInfo& requested, const Protocol::PositionInfo& authoritative)
{
    constexpr float kPosEps = 0.01f;
    constexpr float kYawEps = 1.0f;

    if (std::fabs(requested.x() - authoritative.x()) > kPosEps) return true;
    if (std::fabs(requested.y() - authoritative.y()) > kPosEps) return true;
    if (std::fabs(requested.z() - authoritative.z()) > kPosEps) return true;
    if (GetYawDelta(requested.yaw(), authoritative.yaw()) > kYawEps) return true;
    if (requested.state() != authoritative.state()) return true;
    if (requested.actionstate() != authoritative.actionstate()) return true;
    return false;
}

static bool NeedsNavAdjustOwnerCorrection(const Protocol::PositionInfo& requested, const Protocol::PositionInfo& authoritative)
{
    constexpr float kHorizontalEps = 0.01f;
    constexpr float kYawEps = 1.0f;
    constexpr float kVerticalEps = 0.20f;

    if (std::fabs(requested.x() - authoritative.x()) > kHorizontalEps) return true;
    if (std::fabs(requested.z() - authoritative.z()) > kHorizontalEps) return true;
    if (std::fabs(requested.y() - authoritative.y()) > kVerticalEps) return true;
    if (GetYawDelta(requested.yaw(), authoritative.yaw()) > kYawEps) return true;
    if (requested.state() != authoritative.state()) return true;
    if (requested.actionstate() != authoritative.actionstate()) return true;
    return false;
}

static void LogOwnerCorrection(uint64 playerId,
    const char* reason,
    const Protocol::PositionInfo& requested,
    const Protocol::PositionInfo& authoritative)
{
    std::cout << " [MOVE_OWNER_CORRECTION] reason=" << reason
        << " ID: " << playerId
        << " req=(" << requested.x() << "," << requested.y() << "," << requested.z() << ", yaw=" << requested.yaw() << ")"
        << " srv=(" << authoritative.x() << "," << authoritative.y() << "," << authoritative.z() << ", yaw=" << authoritative.yaw() << ")"
        << std::endl;
}

void GameRoom::SendMoveSync(PlayerSessionRef ownerSession,
    PlayerRef player,
    const Protocol::PositionInfo& posInfo,
    bool sendToOwner,
    bool sendToVisibleOthers)
{
    if (!player)
        return;

    Protocol::S_MOVE movePkt;
    const uint64 playerId = player->GetPlayerId();
    movePkt.set_objectid(playerId);
    *movePkt.mutable_posinfo() = posInfo;

    SendBufferRef sb = ClientPacketHandler::MakeSendBuffer(movePkt);

    if (sendToOwner && ownerSession)
        ownerSession->Send(sb);

    if (!sendToVisibleOthers)
        return;

    if (ExperimentUtils::IsHotRoomRoomWideBaseline())
    {
        const int32 recipients = Broadcast(sb, playerId);
        GameMetrics::OnBroadcastRecipients(
            GameMetrics::HotRoomBroadcastKind::Move,
            GameMetrics::HotRoomBroadcastMode::Room,
            static_cast<std::size_t>(recipients));
        return;
    }

    auto& vis = player->VisiblePlayers_ActorOnly();
    std::size_t recipients = 0;
    for (uint64 vid : vis)
    {
        if (vid == playerId) continue;
        SendToPlayer(vid, sb);
        ++recipients;
    }

    GameMetrics::OnBroadcastRecipients(
        GameMetrics::HotRoomBroadcastKind::Move,
        GameMetrics::HotRoomBroadcastMode::Aoi,
        recipients);
}

// 클라이언트 이동 패킷(C_MOVE) 처리 메인 함수
void GameRoom::HandleMove(PlayerSessionRef session, PlayerRef player, Protocol::C_MOVE pkt)
{
    if (!session || !player) return;

    const uint64 playerId = player->GetPlayerId();
    if (_players.find(playerId) == _players.end()) return;

    // 사망 상태면 이동 차단 (클라 오입력/핵 방지)
    if (auto* st = player->GetStatInfo())
    {
        if (st->hp() <= 0)
            return;
    }
    if (player->GetPosInfo() && player->GetPosInfo()->actionstate() == Protocol::ACTION_DEAD)
        return;

    // Step 0: 입력 데이터 정상성 검사
    // 이상한 좌표 들어오면 로그 찍고 무시함
    const auto& reqRaw = pkt.posinfo();
    if (!IsFinitePos(reqRaw) || IsCrazyPos(reqRaw))
    {
        std::cout << " [MOVE_INVALID_INPUT] ID: " << playerId << std::endl;
        return;
    }

    const Protocol::PositionInfo* curPtr = player->GetPosInfo();
    if (!curPtr)
        return;

    // 현재 서버가 알고 있는 플레이어 위치 (Server Authority)
    const Protocol::PositionInfo cur = *curPtr;
    if (!IsFinitePos(cur))
        return;

    const uint32 seq = pkt.move_seq();
    const uint32 tms = pkt.client_time_ms();
    const uint64 nowMs = ::GetTickCount64();

    // Step 1: 패킷 순서 및 시간 검증 (스피드핵 방지)
    const bool hasStamp = player->HasMoveStamp_ActorOnly();

    if (hasStamp)
    {
        const uint32 lastSeq = player->LastMoveSeq_ActorOnly();
        // UDP처럼 패킷이 뒤집혀서 올 수 있으니 이전 시퀀스 패킷은 버림
        if (!MoveValidate::IsSeqNewer(seq, lastSeq))
        {
            if (seq == lastSeq)
            {
                // 중복 패킷은 조용히 무시
                return;
            }

            std::cout << " [MOVE_SEQ_REWIND_DROP] ID: " << playerId
                << " seq=" << seq << " last=" << lastSeq << std::endl;
            return;
        }
    }

    if (_grid.GetZoneIndex(reqRaw) < 0)
    {
        std::cout << " [MOVE_INVALID_INPUT] OUT_OF_BOUNDS ID: " << playerId << std::endl;
        if (NeedsOwnerCorrection(reqRaw, cur))
        {
            LogOwnerCorrection(playerId, "VALIDATION_REJECT", reqRaw, cur);
            SendMoveSync(session, player, cur, true, false);
        }
        player->SetMoveStamp_ActorOnly(seq, tms, cur, nowMs);
        return;
    }

    // 지난번 이동 패킷과의 시간 차이(dt) 계산
    const float dtSec = MoveValidate::ComputeDtSec(
        tms,
        player->LastClientTimeMs_ActorOnly(),
        0.02f, 0.25f, hasStamp);

    // 플레이어 이동 속도 가져오기
    float speed = 0.f;
    if (auto* st = player->GetStatInfo())
        speed = static_cast<float>(st->speed());

    // 스피드핵 체크: 이론상 갈 수 있는 거리보다 더 많이 갔으면 보정(Clamp)함
    Protocol::PositionInfo reqClamped;
    const auto speedRes = MoveValidate::CheckSpeed2D(
        cur, reqRaw, dtSec, speed, 0.30f, reqClamped);

    if (speedRes.policy == MoveValidate::SpeedPolicy::CLAMPED)
    {
        // 핵 의심되면 로그 남김
        std::cout << " [SPEED_EXCEEDED_CLAMP] ID: " << playerId
            << " reqDist=" << speedRes.reqDist2D
            << " maxDist=" << speedRes.maxDist
            << " dt=" << speedRes.dtSec << std::endl;
    }

    // Step 2~5: NavMesh 검증 (지형지물 통과 방지)
    // 서버에서 길찾기 돌려서 갈 수 있는 곳인지 최종 확인
    Protocol::PositionInfo fixed;
    if (!_map || _map->ValidateMove(cur, reqClamped, fixed) == false)
    {
        std::cout << " [FAIL] ID: " << playerId << " blocked by NavMesh!" << std::endl;
        if (NeedsOwnerCorrection(reqRaw, cur))
        {
            LogOwnerCorrection(playerId, "VALIDATION_REJECT", reqRaw, cur);
            SendMoveSync(session, player, cur, true, false);
        }
        player->SetMoveStamp_ActorOnly(seq, tms, cur, nowMs);
        return;
    }

    // NavMesh 검증된 위치에다가 상태 정보(State, Action, Yaw) 덮어씌움
    fixed.set_state(reqClamped.state());
    fixed.set_actionstate(reqClamped.actionstate());
    fixed.set_yaw(reqClamped.yaw());

    // Step 6: 실제 위치 반영 및 Zone 갱신
    const int32 oldZoneIndex = player->GetZoneIndex();
    const int32 newZoneIndex = _grid.GetZoneIndex(fixed);

    // 안전장치
    if (newZoneIndex < 0)
        return;

    player->SetPosInfo(fixed);

    const bool zoneChanged = (oldZoneIndex != newZoneIndex);

    // Zone이 바뀌었으면 플레이어를 해당 Zone 리스트로 옮겨줌 (Grid 시스템 갱신)
    if (zoneChanged)
    {
        Zone& oldZone = _grid.GetZone(oldZoneIndex);
        Zone& newZone = _grid.GetZone(newZoneIndex);
        oldZone.players.erase(player);
        newZone.players.insert(player);
        player->SetZoneIndex(newZoneIndex);
    }

    // 시야 처리(AOI) 업데이트가 필요한지 체크
    if (!ExperimentUtils::IsHotRoomRoomWideBaseline() && ShouldUpdateAOI(player, zoneChanged))
        UpdateAOI(session, player, false);

    // 이동 패킷 브로드캐스팅: 내 주변에 있는(나를 보고 있는) 플레이어들에게만 전송
    SendMoveSync(session, player, fixed, false, true);

    const bool needsAcceptedCorrection =
        (speedRes.policy == MoveValidate::SpeedPolicy::CLAMPED)
        ? NeedsOwnerCorrection(reqRaw, fixed)
        : NeedsNavAdjustOwnerCorrection(reqRaw, fixed);

    if (needsAcceptedCorrection)
    {
        const char* reason = (speedRes.policy == MoveValidate::SpeedPolicy::CLAMPED)
            ? "SPEED_CLAMP"
            : "NAV_ADJUST";
        LogOwnerCorrection(playerId, reason, reqRaw, fixed);
        SendMoveSync(session, player, fixed, true, false);
    }

    // 마지막으로 검증 완료된 정보를 타임스탬프와 함께 저장 (다음 이동 검증 때 씀)
    player->SetMoveStamp_ActorOnly(seq, tms, fixed, nowMs);
}


void GameRoom::HandleMoveById(PlayerSessionRef session, uint64 playerId, Protocol::C_MOVE pkt)
{
    auto it = _players.find(playerId);
    if (it == _players.end())
        return;

    HandleMove(session, it->second, pkt); // 기존 로직 재활용
}

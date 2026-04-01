#include "pch.h"
#include "GameRoom.h"
#include "Player.h"
#include "PlayerSession.h"
#include "RoomManager.h"
#include "GameRoom.Net.h"
#include "Monster.h"
#include "PartyActor.h"
#include "Projectile.h"
#include "ExperimentUtils.h"
#include "GameMetrics.h"

// 플레이어를 방 자료구조에 실제로 등록하는 내부 함수
// (일반 입장, 맵 이동 입장 공통 사용)
bool GameRoom::EnterRegister(PlayerSessionRef session, PlayerRef player)
{
	printf("1번 여기가 문제임\n");
	if (player == nullptr) return false;

	// 방이 닫히는 중이면 입장 불가
	if (IsClosing())
		return false;

	player->SetInstanceId(_instanceId);

	// 중복 입장 체크 (이미 방에 존재하면 거부)
	if (_players.find(player->GetPlayerId()) != _players.end())
		return false;

	// 1. 룸 소속 설정 및 세션 연결
	// Player 객체에 현재 방 정보를 세팅해줘야 이동/전투 가능
	player->SetRoom(shared_from_this());

	auto room = shared_from_this();
	session->Post([room](PlayerSessionRef ps)
		{
			ps->SetCurrentRoom(room);
		});

	// 플레이어 목록(Map)에 추가
	_players.insert({ player->GetPlayerId(), player });


	// 2. 동시성 제어 및 방 수명 관리
	// 플레이어 수 증가 (Atomic 연산으로 Thread-Safe 하게)
	_playerCount.fetch_add(1, std::memory_order_acq_rel);
	_emptySinceMs.store(0, std::memory_order_release); // 방이 비어있는 시간 초기화

	// 3. Grid / AOI 시스템 등록 [중요]
	// 좌표 기반으로 현재 Zone Index 계산
	int32 zoneIndex = _grid.GetZoneIndex(*player->GetPosInfo());
	player->SetZoneIndex(zoneIndex);

	// 해당 Zone에 플레이어 포인터 등록 (이때부터 다른 유저 눈에 보일 후보가 됨)
	Zone& enterZone = _grid.GetZone(zoneIndex);
	enterZone.players.insert(player);

	printf(" [EnterRegister] Player %llu Zone[%d] at (%.1f, %.1f, %.1f)\n",
		player->GetPlayerId(), zoneIndex,
		player->GetPosInfo()->x(),
		player->GetPosInfo()->y(),
		player->GetPosInfo()->z());

	return true;
}

// [일반 입장] 로그인 후 로비에서 게임 진입 시 호출
void GameRoom::Enter(PlayerSessionRef session, PlayerRef player)
{
	if (!session || !player)
		return;

	// 자료구조 등록 시도
	if (EnterRegister(session, player) == false)
		return;


	if (player == nullptr) return;

	// 1. 입장 성공 패킷 전송 (클라이언트 로딩 해제용)
	Protocol::S_ENTER_GAME enterPkt;
	enterPkt.set_success(true);
	enterPkt.mutable_myplayer()->CopyFrom(*player->GetPlayerInfo());
	enterPkt.set_gold(player->GetGold());
	// enterPkt.set_mapid(_mapId);

	session->Send(ClientPacketHandler::MakeSendBuffer(enterPkt));

	// 2. AOI 업데이트 및 주변 스폰 전송
	// forceFullSnapshot=true: 처음 들어왔으니 주변 모든 객체 정보를 한 번에 받아야 함
	if (ExperimentUtils::IsHotRoomRoomWideBaseline())
	{
		BuildRoomWideVisibilityForPlayer_ActorOnly(player);
		SendRoomWideSnapshotToPlayer_ActorOnly(session, player, true);
		BroadcastRoomWidePlayerSpawn_ActorOnly(player);
	}
	else
	{
		UpdateAOI(session, player, true /*forceFullSnapshot*/);
	}

	printf(" [Enter-Login] Player %llu\n", player->GetPlayerId());
}

// [맵 이동 입장] 던전 진입이나 텔레포트 시 호출
void GameRoom::EnterMapChange(PlayerSessionRef session, PlayerRef player)
{
	printf("2번 여기가 문제임\n");
	if (!session || !player)
		return;

	// 등록 실패하면 맵 이동 상태(MapChanging)를 반드시 풀어줘야 락이 안 걸림
	if (EnterRegister(session, player) == false)
	{
		session->Post([](PlayerSessionRef ps)
			{
				ps->CancelMapChange();
			});
		return;
	}

	const uint64 playerId = player->GetPlayerId();

	// 1. S_MAP_CHANGE_END 전송 (Handshaking 완료)
	// 클라이언트가 보낸 토큰을 다시 돌려줘서 위변조 검증
	Protocol::S_MAP_CHANGE_END endPkt;
	endPkt.set_token(session->GetMapChangeToken());
	endPkt.set_mapid(_mapId);
	endPkt.mutable_pos()->CopyFrom(*player->GetPosInfo());
	endPkt.set_instanceid(_instanceId);
	endPkt.set_targetchannelid(_channelId);

	session->Send(ClientPacketHandler::MakeSendBuffer(endPkt));

	// 2. 스폰 패킷 전송 (내 주변 몹/유저 정보)
	// 맵 로딩이 끝난 직후이므로 풀 스냅샷 전송
	if (ExperimentUtils::IsHotRoomRoomWideBaseline())
	{
		BuildRoomWideVisibilityForPlayer_ActorOnly(player);
		SendRoomWideSnapshotToPlayer_ActorOnly(session, player, true);
		BroadcastRoomWidePlayerSpawn_ActorOnly(player);
	}
	else
	{
		UpdateAOI(session, player, true /*forceFullSnapshot*/);
	}

	// 3. 파티 정보 재전송
	// 맵 이동 시 파티 UI가 갱신되어야 하므로 파티 액터에게 정보 요청
	PartyActor::Instance().Push([session, playerId]()
		{
			auto& core = PartyActor::Instance().Core();
			const uint64 partyId = core.GetPartyIdByPlayerId(playerId);

			if (partyId != 0)
			{
				auto snap = core.GetSnapshot(partyId);
				if (snap.partyId != 0)
				{
					Protocol::S_PARTY_INFO_NTF info;
					info.set_hasparty(true);
					info.set_partyid(snap.partyId);
					info.set_leaderid(snap.leaderId);
					info.set_version(snap.version);
					for (uint64 id : snap.members)
						info.add_memberids(id);

					session->Post([info](PlayerSessionRef s) mutable
						{
							s->Send(ClientPacketHandler::MakeSendBuffer(info));
						});
				}
			}
		});

	printf(" [MapChange-END] Player %llu -> Map %d (Inst=%lld) Token=%llu Channel=%d\n",
		playerId, _mapId, (long long)_instanceId, endPkt.token(), endPkt.targetchannelid());
}

// [퇴장 처리] 로그아웃 혹은 맵 이동으로 방을 떠날 때
void GameRoom::Leave(PlayerSessionRef session, PlayerRef player)
{
	if (!session || !player) return;

	// [Trade] 거래 중이었다면 강제 취소 처리
	// 그냥 나가면 아이템이 증발하거나 복사될 수 있으므로 예외 처리 필수
	const uint64 tradeId = player->ActiveTradeId_ActorOnly();
	if (tradeId != 0)
	{
		CancelTrade_ActorOnly(tradeId, Protocol::TRADE_CANCEL_DISCONNECT);
	}

	const uint64 meId = player->GetPlayerId();
	auto itMe = _players.find(meId);
	if (itMe == _players.end()) return;

	// 1. 주변 시야 정리 (Despawn 전파)
	// 나를 보고 있던 유저들에게 "나 사라짐" 패킷을 보내고, 
	// 그들의 Visible List에서 나를 지워야 함 (Zombie 객체 방지)
	if (ExperimentUtils::IsHotRoomRoomWideBaseline())
	{
		Protocol::S_DESPAWN pkt;
		pkt.add_objectids(meId);
		SendBufferRef sb = ClientPacketHandler::MakeSendBuffer(pkt);

		const int32 recipients = Broadcast(sb, meId);
		GameMetrics::OnBroadcastRecipients(
			GameMetrics::HotRoomBroadcastKind::Despawn,
			GameMetrics::HotRoomBroadcastMode::Room,
			static_cast<std::size_t>(recipients));

		ClearRoomWideVisibilityForPlayer_ActorOnly(player);
	}
	else
	{
		Protocol::S_DESPAWN pkt;
		pkt.add_objectids(meId);
		SendBufferRef sb = ClientPacketHandler::MakeSendBuffer(pkt);

		auto& visP = player->VisiblePlayers_ActorOnly();
		std::size_t recipients = 0;
		for (uint64 vid : visP)
		{
			PlayerRef other = FindPlayer_ActorOnly(vid);
			if (!other) continue;
			other->VisiblePlayers_ActorOnly().erase(meId);
			SendToPlayer(vid, sb);
			++recipients;
		}
		visP.clear();
		GameMetrics::OnBroadcastRecipients(
			GameMetrics::HotRoomBroadcastKind::Despawn,
			GameMetrics::HotRoomBroadcastMode::Aoi,
			recipients);

		// 몬스터 어그로 목록에서도 제거
		auto& visM = player->VisibleMonsters_ActorOnly();
		for (uint64 mid : visM)
		{
			auto it = _monsters.find(mid);
			if (it != _monsters.end() && it->second)
				it->second->Viewers_ActorOnly().erase(meId);
		}
		visM.clear();

		// 투사체 뷰어 목록에서도 제거
		auto& visPr = player->VisibleProjectiles_ActorOnly();
		for (uint64 prid : visPr)
		{
			auto it = _projectiles.find(prid);
			if (it != _projectiles.end() && it->second)
				it->second->Viewers_ActorOnly().erase(meId);
		}
		visPr.clear();
	}

	// 2. Grid 시스템에서 제거
	int32 zoneIndex = player->GetZoneIndex();
	int32 totalZones = _grid.GetGridSizeX() * _grid.GetGridSizeY();
	if (zoneIndex >= 0 && zoneIndex < totalZones)
		_grid.GetZone(zoneIndex).players.erase(player);

	// 3. 방 자료구조에서 최종 제거
	_players.erase(meId);
	player->SetRoom(nullptr);

	// 세션에서도 현재 방 정보 해제
	auto room = shared_from_this();
	session->Post([room](PlayerSessionRef ps) { ps->ClearCurrentRoom(room); });

	// 플레이어 수 감소 및 방이 비었는지 체크
	const int32 after = _playerCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
	if (after == 0)
		_emptySinceMs.store(::GetTickCount64(), std::memory_order_release);
}

void GameRoom::LeaveById(PlayerSessionRef session, uint64 playerId)
{
	auto it = _players.find(playerId);
	if (it == _players.end())
		return;

	PlayerRef player = it->second;

	// 공통 Leave 로직 재사용
	Leave(session, player);
}

PlayerRef GameRoom::FindPlayer_ActorOnly(uint64 playerId) const
{
	auto it = _players.find(playerId);
	if (it == _players.end())
		return nullptr;
	return it->second;
}

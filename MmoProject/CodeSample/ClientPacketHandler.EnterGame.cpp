#include "pch.h"
#include "ClientPacketHandler.h"
#include "S2SPacketHandler.h" 
#include "PlayerSession.h"
#include "GameSessionManager.h"
#include "RedisManager.h"
#include "DataManager.h"
#include "LobbyRoom.h"
#include "RoomManager.h"
#include "PartyActor.h"
#include "ExperimentUtils.h"

extern shared_ptr<PacketSession> G_DBSession;

// 로그인 후 게임 진입을 요청하는 핵심 핸들러
// 레디스 인증, 세션 바인딩, DB 로딩 요청 등 초기화 작업을 여기서 수행함
bool ClientPacketHandler::Handle_C_ENTER_GAME(PacketSessionRef& session, Protocol::C_ENTER_GAME& pkt)
{
	PlayerSessionRef ps = static_pointer_cast<PlayerSession>(session);
	if (!ps) return false;

	// 레디스에 저장된 토큰을 검증해서 유효한 접속인지 확인
	// 보안상 중요한 부분이므로 토큰이 없으면 연결 끊음
	std::string token = pkt.token();
	std::string value = GRedisManager->Get(token);

	if (value.empty())
	{
		printf(" [EnterGame] Invalid Token: %s\n", token.c_str());
		ps->Disconnect(L"Invalid Token");
		return false;
	}

	uint64 playerId = std::stoull(value);
	std::string playerName = GRedisManager->Get("token:name:" + token);

	int32 channelId = pkt.channelid();
	if (channelId <= 0) channelId = 1;

	int32 mapId = pkt.mapid();
	DataManager* dm = DataManager::Instance();

	// 게임 진입은 기본적으로 월드맵으로만 허용한다
	// 던전이나 잘못된 맵 ID가 들어오면 기본 맵으로 강제 보정함
	if (!dm)
	{
		mapId = 1;
	}
	else
	{
		if (!dm->IsValidMapId(mapId) || !dm->IsWorldMapId(mapId))
			mapId = dm->GetDefaultWorldMapId();

		// 방어 코드: 맵 설정 파일이 없어도 기본 맵으로 보냄
		if (dm->GetMapConfig(mapId) == nullptr)
			mapId = dm->GetDefaultWorldMapId();
	}

	const int32 requestedWorldMapId = mapId;
	mapId = ExperimentUtils::ResolveForcedWorldMapId(mapId);
	if (requestedWorldMapId != mapId)
	{
		printf(" [EnterGame] Experiment map override: %d -> %d\n", requestedWorldMapId, mapId);
	}

	const MapConfig* cfg = (dm ? dm->GetMapConfig(mapId) : nullptr);


	// 입장할 위치 좌표 설정
	Protocol::PositionInfo spawn;
	spawn.set_x(cfg ? cfg->spawnX : 50.f);
	spawn.set_y(cfg ? cfg->spawnY : 0.f);
	spawn.set_z(cfg ? cfg->spawnZ : 50.f);

	// ForceReturn 플래그가 있으면 로그인 시점에 기본 월드로 강제 귀환 처리
	PartyActor::Instance().Push([ps, playerId, channelId, mapId, spawn, playerName]() mutable
		{
			auto& core = PartyActor::Instance().Core();
			const bool forceReturn = core.ConsumeForceReturn(playerId);

			int32 finalMapId = mapId;
			Protocol::PositionInfo finalSpawn = spawn;

			if (forceReturn)
			{
				DataManager* dm = DataManager::Instance();
				finalMapId = (dm ? dm->GetDefaultWorldMapId() : 1);
				finalMapId = ExperimentUtils::ResolveForcedWorldMapId(finalMapId);

				const MapConfig* cfg = dm ? dm->GetMapConfig(finalMapId) : nullptr;
				finalSpawn.Clear();
				finalSpawn.set_x(cfg ? cfg->spawnX : 50.f);
				finalSpawn.set_y(cfg ? cfg->spawnY : 0.f);
				finalSpawn.set_z(cfg ? cfg->spawnZ : 50.f);
			}

			// 여기서부터는 세션 액터의 컨텍스트로 전환
			// 세션에 플레이어 ID를 묶고, 로비 룸을 찾아 플레이어 객체 생성을 위임함
			ps->Post([playerId, channelId, finalMapId, finalSpawn, playerName](PlayerSessionRef ps) mutable
				{
					GameSessionManager::GSessionManager->BindPlayerId(ps, playerId);
					ps->SetPlayerId_ActorOnly(playerId);

					// DB 응답이 왔을 때 어떤 채널/맵으로 가야 할지 알기 위해 미리 저장해둠
					// 이 정보가 없으면 DB 로딩 후 미아가 될 수 있음
					ps->SetPendingEnter_ActorOnly(channelId, finalMapId, /*instanceId=*/0);

					// 채널별로 존재하는 로비 룸에 입장 요청을 보냄
					if (GRoomManager)
					{
						auto lobby = GRoomManager->GetOrCreateLobby(channelId);
						if (lobby)
						{
							lobby->Push([ps, playerId, channelId, finalMapId, finalSpawn, playerName, lobby]() mutable
								{
									lobby->EnterGame(ps, playerId, channelId, finalMapId, finalSpawn, playerName);
								});
						}
					}

					// 플레이어 데이터 로딩을 위해 DB 서버로 비동기 요청 전송
					if (G_DBSession)
					{
						Protocol::S2S_REQ_LOAD_PLAYER_DATA reqStat;
						reqStat.set_playerid(playerId);
						reqStat.set_gamesessionid(ps->GetSessionId());
						G_DBSession->Send(S2SPacketHandler::MakeSendBuffer(reqStat));

						Protocol::S2S_REQ_ITEMS_LOAD reqItem;
						reqItem.set_playerid(playerId);
						reqItem.set_gamesessionid(ps->GetSessionId());
						G_DBSession->Send(S2SPacketHandler::MakeSendBuffer(reqItem));

						Protocol::S2S_REQ_QUICKSLOT_LOAD reqQs;
						reqQs.set_playerid(playerId);
						reqQs.set_gamesessionid(ps->GetSessionId());
						G_DBSession->Send(S2SPacketHandler::MakeSendBuffer(reqQs));
					}
				});
		});


	return true;
}

// 로그인 패킷은 별도 인증 서버를 타거나 다른 방식을 써서 여기선 처리 안 함
bool ClientPacketHandler::Handle_C_LOGIN(PacketSessionRef& session, Protocol::C_LOGIN& pkt)
{
	return false;
}

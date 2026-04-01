#include "pch.h"
#include "ThreadManager.h"
#include "Service.h"
#include "Session.h"
#include "PlayerSession.h"
#include "BufferWriter.h"
#include "ClientPacketHandler.h"
#include "S2SPacketHandler.h"
#include "DBSession.h"
#include "GameRoom.h" 
#include <iostream>
#include <windows.h>
#include "LoginSession.h"
#include "RoomManager.h" 
#include "DataManager.h"
#include "LobbyRoom.h"
#include "GameMetrics.h"
#include <fstream> 
#include "PersistenceService.h"
#include "AutoCommitService.h"

// 다른 소스 파일에 정의된 전역 객체들을 여기서 참조함
// 로그인 세션, DB 세션, 실행 플래그, Redis 매니저 등 서버 전역 상태를 관리하는 변수들임
extern shared_ptr<LoginSession> G_LoginSession;
extern std::shared_ptr<PacketSession> G_DBSession;
extern std::atomic<bool> GIsRunning;
extern RedisManager* GRedisManager;


// 윈도우 콘솔 종료 이벤트 핸들러
// 서버를 강제로 끌 때(Ctrl+C), 데이터 저장이나 스레드 종료를 안전하게 처리하기 위해 플래그를 내림
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
		std::cout << " [System] Server Shutdown Initiated..." << std::endl;
		GIsRunning = false;
		return TRUE;
	default:
		return FALSE;
	}
}


//=========================================임시 콘솔 테스트==========================================
#include <sstream>
#include <string>
#include "GameSessionManager.h"

// 클라이언트 없이 서버 로직만 테스트하기 위한 콘솔 명령어 처리 스레드
// 파티 시스템처럼 복잡한 상호작용은 클라 2~3개 띄우기 귀찮으니까 여기서 텍스트 명령어로 시뮬레이션함
void ConsoleThread()
{
	while (GIsRunning)
	{
		std::string line;
		std::getline(std::cin, line);
		if (line.empty()) continue;

		std::istringstream iss(line);
		std::string cmd;
		iss >> cmd;

		// 세션 매니저에서 PlayerID로 세션을 찾는 람다 함수
		// 테스트 명령어의 첫 번째 인자는 무조건 대상 플레이어 ID라고 가정하고 짬
		auto GetSession = [&](uint64 pid) -> PlayerSessionRef {
			auto session = GameSessionManager::GSessionManager->FindByPlayerId(pid);
			if (session == nullptr) {
				std::cout << " [Test] Player " << pid << " not found in SessionManager!" << std::endl;
			}
			return session;
			};

		if (cmd == "/dummy") // 더미 세션 생성 (네트워크 연결 없이 로직 객체만 생성)
		{
			uint64 pid; iss >> pid;
			// 실제 소켓 연결은 없지만 메모리 상에 세션 객체를 만들어서 로직 테스트용으로 씀
			auto dummySession = MakeShared<PlayerSession>();
			// 주의: Send를 호출하면 소켓이 없어서 터질 수 있으니 로직 검증용으로만 사용해야 함
		}

		// 파티 시스템 테스트 명령어 모음
		// 패킷 핸들러를 직접 호출해서 클라이언트가 패킷을 보낸 것처럼 속임
		if (cmd == "/p_create") // 파티 생성 요청 시뮬레이션
		{
			uint64 pid; iss >> pid;
			auto session = GetSession(pid);
			if (session) {
				Protocol::C_PARTY_CREATE_REQ pkt;
				PacketSessionRef ps = static_pointer_cast<PacketSession>(session);
				ClientPacketHandler::Handle_C_PARTY_CREATE_REQ(ps, pkt);
			}
		}
		else if (cmd == "/p_invite") // 파티 초대 요청
		{
			uint64 inviterId, targetId;
			iss >> inviterId >> targetId;
			auto session = GetSession(inviterId);
			if (session) {
				Protocol::C_PARTY_INVITE_REQ pkt;
				pkt.set_targetplayerid(targetId);
				PacketSessionRef ps = static_pointer_cast<PacketSession>(session);
				ClientPacketHandler::Handle_C_PARTY_INVITE_REQ(ps, pkt);
			}
		}
		else if (cmd == "/p_accept") // 파티 초대 수락/거절
		{
			uint64 pid, partyId; bool accept;
			iss >> pid >> partyId >> accept;
			auto session = GetSession(pid);
			if (session) {
				Protocol::C_PARTY_INVITE_ACCEPT_REQ pkt;
				pkt.set_partyid(partyId);
				pkt.set_accept(accept);
				PacketSessionRef ps = static_pointer_cast<PacketSession>(session);
				ClientPacketHandler::Handle_C_PARTY_INVITE_ACCEPT_REQ(ps, pkt);
			}
		}
		else if (cmd == "/p_status") // 내 파티 상태 확인
		{
			uint64 pid; iss >> pid;
			auto session = GetSession(pid);
			if (session) {
				Protocol::C_PARTY_STATUS_REQ pkt;
				PacketSessionRef ps = static_pointer_cast<PacketSession>(session);
				ClientPacketHandler::Handle_C_PARTY_STATUS_REQ(ps, pkt);
			}
		}
		else if (cmd == "/p_leave") // 파티 탈퇴
		{
			uint64 pid; iss >> pid;
			auto session = GetSession(pid);
			if (session) {
				Protocol::C_PARTY_LEAVE_REQ pkt;
				PacketSessionRef ps = static_pointer_cast<PacketSession>(session);
				ClientPacketHandler::Handle_C_PARTY_LEAVE_REQ(ps, pkt);
			}
		}
		else if (cmd == "/p_kick") // 파티원 추방 (파티장 전용)
		{
			uint64 leaderId, targetId;
			iss >> leaderId >> targetId;
			auto session = GetSession(leaderId);
			if (session) {
				Protocol::C_PARTY_KICK_REQ pkt;
				pkt.set_targetplayerid(targetId);
				PacketSessionRef ps = static_pointer_cast<PacketSession>(session);
				ClientPacketHandler::Handle_C_PARTY_KICK_REQ(ps, pkt);
			}
		}
		else if (cmd == "/p_disband") // 파티 해산
		{
			uint64 pid; iss >> pid;
			auto session = GetSession(pid);
			if (session) {
				Protocol::C_PARTY_DISBAND_REQ pkt;
				PacketSessionRef ps = static_pointer_cast<PacketSession>(session);
				ClientPacketHandler::Handle_C_PARTY_DISBAND_REQ(ps, pkt);
			}
		}
	}
}

int main()
{
	SetConsoleCtrlHandler(CtrlHandler, TRUE);

	GameMetrics::Initialize();

	// 패킷 핸들러 테이블 초기화 (함수 포인터 매핑)
	ClientPacketHandler::Init();
	S2SPacketHandler::Init();

	{
		// 1. 기획 데이터(JSON) 로딩
		// 서버 실행 파일 옆에 Maps.json이 있는지 체크하고, NavMesh 및 맵 속성 정보를 메모리에 올림
		std::ifstream ifs("Maps.json");
		if (!ifs.is_open())
		{
			std::cout << " [GameServer] Maps.json not found (expected next to exe). "
				"Fallback InitMapRegistry() will be used.\n";
		}
		else
		{
			std::cout << " [GameServer] Maps.json found.\n";
		}

		// 실제 로딩은 싱글톤인 DataManager가 수행함
		DataManager* dm = DataManager::Instance();
		if (!dm->LoadMapConfigsFromJson("Maps.json"))
		{
			std::cout << " [GameServer] Maps.json load failed. fallback InitMapRegistry() will be used.\n";
		}
		else
		{
			std::cout << " [GameServer] Maps.json loaded.\n";
		}

		// 2. 몬스터/스폰/드랍 데이터 로딩
		std::ifstream ifsMonster("MonsterTemplates.json");
		if (!ifsMonster.is_open())
		{
			std::cout << " [GameServer] MonsterTemplates.json not found (expected next to exe).\n";
		}
		else
		{
			std::cout << " [GameServer] MonsterTemplates.json found.\n";
		}

		if (!dm->LoadMonsterTemplatesFromJson("MonsterTemplates.json"))
		{
			std::cout << " [GameServer] MonsterTemplates.json load failed.\n";
		}

		std::ifstream ifsSpawn("SpawnTables.json");
		if (!ifsSpawn.is_open())
		{
			std::cout << " [GameServer] SpawnTables.json not found (expected next to exe).\n";
		}
		else
		{
			std::cout << " [GameServer] SpawnTables.json found.\n";
		}

		if (!dm->LoadSpawnTablesFromJson("SpawnTables.json"))
		{
			std::cout << " [GameServer] SpawnTables.json load failed.\n";
		}

		std::ifstream ifsDrop("DropTables.json");
		if (!ifsDrop.is_open())
		{
			std::cout << " [GameServer] DropTables.json not found (expected next to exe).\n";
		}
		else
		{
			std::cout << " [GameServer] DropTables.json found.\n";
		}

		if (!dm->LoadDropTablesFromJson("DropTables.json"))
		{
			std::cout << " [GameServer] DropTables.json load failed.\n";
		}
	}


	// IOCP 코어 및 세션 매니저 생성
	IocpCoreRef core = MakeShared<IocpCore>();

	GameSessionManager::GSessionManager = xnew<GameSessionManager>();

	// 서비스 객체 생성 (포트 분리 전략)
	// DB 서버 통신용 서비스 (포트 7778)
	ClientServiceRef dbService = MakeShared<ClientService>(
		NetAddress(L"127.0.0.1", 7778),
		core,
		MakeShared<DBSession>,
		1
	);

	// 채팅 서버용 서비스 (현재는 주석 처리됨)
	/*ClientServiceRef chatService = MakeShared<ClientService>(
		NetAddress(L"127.0.0.1", 7776),
		core,
		MakeShared<ChatSession>,
		1
	);*/

	// 게임 클라이언트 접속용 서비스 (포트 7777, 최대 접속 1000명)
	ServerServiceRef gameService = MakeShared<ServerService>(
		NetAddress(L"127.0.0.1", 7777),
		core,
		MakeShared<PlayerSession>,
		1000
	);

	// 로그인 서버 통신용 서비스 (포트 7780)
	ClientServiceRef loginService = MakeShared<ClientService>(
		NetAddress(L"127.0.0.1", 7780),
		core,
		MakeShared<LoginSession>,
		1
	);

	// 각 서비스 시작 (Bind -> Listen 혹은 Connect)
	ASSERT_CRASH(dbService->Start());
	ASSERT_CRASH(loginService->Start());
	ASSERT_CRASH(gameService->Start());

	// 룸 매니저 초기화
	GRoomManager = MakeShared<RoomManager>();

	// 영속성 서비스 초기화 (Redis 연결)
	Persistence::PersistenceService::I().Init(GRedisManager);

	// 자동 저장(AutoCommit) 서비스 초기화
	// Redis에 있는 데이터를 주기적으로 DB로 밀어넣는(Flush) 역할을 함
	// 여기서 람다로 패킷 전송 로직을 주입해주는 이유는 AutoCommitService 자체가 네트워크 세션을 모르기 때문임
	Persistence::AutoCommitService::I().Init(
		GRedisManager,
		[](const Protocol::S2S_REQ_SAVE_PLAYER_CORE& pkt)
		{
			if (!G_DBSession) return;
			auto tmp = pkt;
			auto sb = S2SPacketHandler::MakeSendBuffer(tmp);
			G_DBSession->Send(sb);
		},
		[](const Protocol::S2S_REQ_SAVE_INVENTORY& pkt)
		{
			if (!G_DBSession) return;
			auto tmp = pkt;
			auto sb = S2SPacketHandler::MakeSendBuffer(tmp);
			G_DBSession->Send(sb);
		},
		[](const Protocol::S2S_REQ_SAVE_QUICKSLOT& pkt)
		{
			if (!G_DBSession) return;
			auto tmp = pkt;
			auto sb = S2SPacketHandler::MakeSendBuffer(tmp);
			G_DBSession->Send(sb);
		}

	);

	// 자동 저장 스레드 시작
	Persistence::AutoCommitService::I().Start();

	// 테스트용 콘솔 스레드 시작
	GThreadManager->Launch([=]() { ConsoleThread(); });

	std::cout << " [GameServer] Running... (Press Ctrl+C to quit)" << std::endl;
	std::cout << " [Command] /p_create [pid], /p_invite [inviter] [target], /p_accept [pid] [partyId] [1/0]" << std::endl;

	// 스레드 풀 구성 전략
	// 하드웨어 코어 개수에 맞춰 스레드를 생성하되, IO용과 로직용으로 반반 나눔
	// IO 스레드: IOCP 완료 통지 처리 (네트워크 수신/송신)
	// Logic 스레드: JobQueue에 쌓인 게임 로직 처리
	int32 threadCount = std::thread::hardware_concurrency();
	if (threadCount < 2) threadCount = 2;

	int32 networkThreadCount = threadCount / 2;
	int32 logicThreadCount = threadCount - networkThreadCount;

	for (int32 i = 0; i < networkThreadCount; i++)
	{
		GThreadManager->Launch([=]() {
			while (GIsRunning) { core->Dispatch(10); }
			});
	}

	for (int32 i = 0; i < logicThreadCount; i++)
	{
		GThreadManager->Launch([=]() {
			ThreadManager::DoGlobalQueueWork();
			});
	}

	// 메인 루프 (Main Loop)
	// 서버의 심장박동 역할을 함. 게임 룸들의 Update()를 주기적으로 호출해서
	// 몬스터 AI, 버프 틱, 쿨타임 등을 처리함
	// 50ms마다 틱을 돌리므로 서버의 기본 프레임레이트는 20FPS가 됨

	uint64 lastHeartbeatTick = 0;

	while (GIsRunning)
	{
		// 1. 모든 게임 룸 업데이트 (가장 중요한 부분)
		// 여기서 각 방의 JobQueue에 Update Job을 밀어넣음
		if (GRoomManager)
			GRoomManager->UpdateAll();

		// 2. 프레임 제한 (Sleep을 줘서 CPU 점유율 방어)
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		// 3. 서비스 하트비트 체크
		// 연결 끊김이나 좀비 세션 관리용 (3초에 한 번만 해도 충분)
		uint64 now = ::GetTickCount64();
		if (now - lastHeartbeatTick > 3000)
		{
			lastHeartbeatTick = now;
			// 필요한 경우 서비스별 Health Check 수행
			dbService->CheckHeartbeat();
			loginService->CheckHeartbeat();
			// gameService는 플레이어 클라용 (필요 시만 활성화)
			//gameService->CheckHeartbeat();
		}
	}

	// 서버 종료 시 정리 작업
	GThreadManager->Join();

	xdelete(GameSessionManager::GSessionManager);
	GameSessionManager::GSessionManager = nullptr;


	gameService->CloseService();
	dbService->CloseService();
	loginService->CloseService();

	GameMetrics::Shutdown();

	return 0;
}

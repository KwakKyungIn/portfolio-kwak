#include "pch.h"                             // 미리컴파일 헤더: 빈번한 공용 포함으로 빌드 속도↑
#include "Service.h"                         // ClientService: 소켓 연결/수명 관리
#include "Session.h"                         // Session 기초 인터페이스
#include "ServerPacketHandler.h"             // 패킷 직렬화/핸들러 진입점
#include "Protocol.pb.h"                     // Protobuf 생성 헤더
#include "ServerSession.h"                   // 더미 클라에서 쓰는 세션 타입(상태 플래그 포함)
#include "ThreadManager.h"                   // 워커 스레드 생성/조인 관리
#include <atomic>                            // 종료 플래그 등 원자적 제어
#include <csignal>                           // Ctrl+C(SIGINT) 처리
#include <iostream>                          // 콘솔 로그
#include <string>                            // 이름/메시지 조립
#include <thread>                            // sleep/런루프 스레드
#include <vector>                            // 세션/서비스 컨테이너
#include <chrono>                            // 타이머/주기 스케줄링
#include <random>                            // 랜덤 문자열 생성

using namespace std::chrono;                 // steady_clock/duration 편하게 쓰려고

// ===== 고정 프로파일 =====
static constexpr int   kClients = 500;       // 동시 접속 수(재현성 위해 상수)
static constexpr int   kRooms = 10;        // 전체 방 수(분산 계산에 사용)
static constexpr double kRps = 10.0;       // 1 클라당 초당 채팅 횟수

// === 시뮬레이터 ===
class ClientSimulator
{
public:
    void Start()
    {
        _core = MakeShared<IocpCore>();       // 모든 세션이 하나의 IOCP 큐 공유(핸들/컨텍스트 전환 비용↓)
        _states.reserve(kClients);            // 재할당 줄여서 지연 변동↓
        _services.reserve(kClients);          // 위와 동일 이유

        const auto now = steady_clock::now(); // 기준 시각(스케줄 계산 시작점)

        for (int i = 0; i < kClients; ++i)    // 클라 n개 생성
        {
            const std::string name = "User" + std::to_string(i + 1); // 보기 쉬운 아이디
            auto session = MakeShared<ServerSession>(name);           // 세션 객체(로그인/룸 상태 보유)

            ClientServiceRef service = MakeShared<ClientService>(     // 실제 소켓 연결을 관리
                NetAddress(L"127.0.0.1", 7777),                      // 로컬 서버 포트
                _core,                                                // 공유 IOCP
                [session]() { return session; },                      // 세션 팩토리(연결 시 사용)
                1);                                                   // 서비스 워커 1개(디스패치는 별도 스레드)

            ASSERT_CRASH(service->Start());   // 소켓 연결/IO 시작 보장(실패시 즉시 중단)
            _services.push_back(service);     // 수명 유지 위해 벡터 보관

            ClientState st;                   // 개별 클라 상태(타이머/액션)
            st.sess = session;                // 내가 조작할 대상 세션
            st.period = duration<double>(1.0 / (kRps > 0.0 ? kRps : 1.0)); // 분모 0 방지
            st.nextChat = (steady_clock::time_point::max)();          // 처음엔 채팅 타이머 비활성화

            const int userId = i + 1;         // 1-based로 계산 편하게
            if (userId <= kRooms) {
                st.action = RoomAction::CreateAfterWait;              // 처음 10명은 방장 역할
                st.actionTime = now + seconds(1);                     // 1초 뒤 생성(서버 준비시간 줌)
            }
            else {
                st.action = RoomAction::JoinAfterWait;                // 나머진 입장 역할
                st.actionTime = now + seconds(2);                     // 방이 먼저 생기도록 2초 대기
            }

            _states.push_back(std::move(st)); // 상태 보관(재할당 최소화 위해 reserve 선행)
        }

        _running = true;                       // 런루프 가동 플래그

        [[maybe_unused]] unsigned int hc = std::thread::hardware_concurrency(); // CPU 코어 수 참조용
        [[maybe_unused]] int ioThreads = (hc > 0 ? static_cast<int>(hc) / 4 : 1); // 향후 확장 아이디어(현재 미사용)

        GThreadManager->Launch([this]() {      // IOCP 디스패치 전용 스레드
            while (_running) {
                _core->Dispatch();             // 완료패킷 처리(Recv/Send 콜백)
            }
            });

        GThreadManager->Launch([this]() {      // 시뮬레이터 로직 스레드(게임 루프 느낌)
            RunLoopPartition(0, 1);            // 전체 인덱스를 1개 파티션으로 순회
            });
    }

    void Stop()
    {
        _running = false;                      // 루프 종료 신호
        GThreadManager->Join();                // 위에서 띄운 모든 스레드 합류
    }

    void PumpOnce()
    {
        if (_core) _core->Dispatch();          // 외부에서 단발 디스패치가 필요할 때 사용
    }

private:
    enum class RoomAction { None, CreateAfterWait, JoinAfterWait }; // 1회성 룸 작업 구분

    struct ClientState
    {
        std::shared_ptr<ServerSession>          sess;      // 내가 제어할 세션 핸들
        std::chrono::duration<double>           period{ 1.0 }; // 채팅 주기(초 단위)
        std::chrono::steady_clock::time_point   nextChat{};    // 다음 채팅 시각(타이머)
        bool                                    roomReqInFlight = false; // 중복 요청 방지 플래그

        RoomAction                               action = RoomAction::None; // 예약된 1회 작업
        std::chrono::steady_clock::time_point    actionTime{};              // 실행 예정 시각
        bool                                     joinedOnce = false;        // 첫 입장 감지(확장 여지)
    };

    std::string RandomString()
    {
        static const char charset[] =
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789";                       // 가벼운 랜덤 페이로드

        thread_local static std::mt19937 rng{ std::random_device{}() };
        thread_local static std::uniform_int_distribution<int> distProb(1, 100); // 확률 결정
        thread_local static std::uniform_int_distribution<> distChar(0, sizeof(charset) - 2);

        // 길이 분포 설정
        thread_local static std::uniform_int_distribution<> dist64(60, 64);      // 64B 근처
        thread_local static std::uniform_int_distribution<> dist256(240, 256);   // 256B 근처
        thread_local static std::uniform_int_distribution<> dist1k(1024, 2048);  // 1~2KB

        int p = distProb(rng);
        int length = 0;
        if (p <= 80)        // 80% 확률
            length = dist64(rng);
        else if (p <= 98)   // 다음 18%
            length = dist256(rng);
        else                // 나머지 2%
            length = dist1k(rng);

        std::string result;
        result.reserve(length);
        for (int i = 0; i < length; ++i)
            result.push_back(charset[distChar(rng)]);
        return result;
    }

    void RunLoopPartition(int tid, int tcount)
    {
        while (_running)                       // 종료 신호 들어올 때까지 루프
        {
            const auto now = steady_clock::now(); // 틱 기준 시각

            for (int i = tid; i < static_cast<int>(_states.size()); i += tcount)  // 파티션 스텝 순회
            {
                auto& st = _states[i];         // 상태 참조(복사 방지)
                auto& sess = st.sess;          // 세션 참조
                if (!sess->isLoggedIn) continue; // 로그인 전이면 아무 것도 안 함

                const int userId = i + 1;      // 같은 이유로 1-based

                // 예약된 룸 작업 실행 타이밍 도달 시 1회만 전송
                if (!sess->isInRoom && !st.roomReqInFlight && st.action != RoomAction::None && now >= st.actionTime)
                {
                    if (st.action == RoomAction::CreateAfterWait) {
                        Protocol::C_CREATE_ROOM_REQ pkt;              // 방 생성 요청
                        pkt.set_roomname("Room_" + std::to_string(userId)); // 방 이름 규칙
                        pkt.set_type(Protocol::ROOM_GROUP);           // 그룹 타입(프로토콜 정의에 맞춤)
                        sess->Send(ServerPacketHandler::MakeSendBuffer(pkt)); // 직렬화 + 송신
                    }
                    else { // JoinAfterWait
                        const int targetRoom = ((userId - 1) / 50) + 1; // 50명/방으로 균등 분산(1~10번 방)
                        Protocol::C_JOIN_ROOM_REQ pkt;                 // 방 참가 요청
                        pkt.set_roomid(targetRoom);                    // 목표 방 지정
                        sess->Send(ServerPacketHandler::MakeSendBuffer(pkt)); // 전송
                    }
                    st.roomReqInFlight = true;  // 응답 올 때까지 재요청 방지
                    st.action = RoomAction::None; // 같은 작업 다시 못 타게 리셋
                    continue;                   // 이번 틱은 여기서 마무리
                }

                // 입장이 확인되면(서버 응답 반영) 인플라이트 해제 + 채팅 타이머 가동
                if (sess->isInRoom) {
                    st.roomReqInFlight = false; // 룸 작업 완료 처리
                    if (st.nextChat == (steady_clock::time_point::max)()) { // 아직 비활성 상태면
                        st.nextChat = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(st.period); // 첫 타 설정
                    }
                }

                // 방 안에 있고, 채팅 타이머가 도달했으면 메시지 전송
                if (sess->isInRoom && now >= st.nextChat)
                {
                    int maxBurst = 10;          // 타이머 밀림 시 과도한 몰아치기 방지 상한
                    do {
                        Protocol::C_ROOM_CHAT_REQ pkt;                // 채팅 패킷
                        pkt.set_message(RandomString());              // 랜덤 페이로드
                        sess->Send(ServerPacketHandler::MakeSendBuffer(pkt)); // 송신
                        st.nextChat += std::chrono::duration_cast<std::chrono::steady_clock::duration>(st.period); // 다음 타이머
                    } while (--maxBurst > 0 && now >= st.nextChat);  // catch-up 루프(상한까지)
                }
            }

            std::this_thread::sleep_for(std::chrono::microseconds(200)); // 틱 간격(바쁜 대기 방지)
        }
    }

private:
    IocpCoreRef                   _core;       // 공유 IOCP 코어(완료패킷 큐)
    std::vector<ClientServiceRef> _services;   // 서비스 보관(수명 유지)
    std::vector<ClientState>      _states;     // 각 클라 시뮬 상태
    std::atomic<bool>             _running{ false }; // 종료 제어 플래그
};

// === 종료 핸들러 ===
static std::atomic<bool> g_running = true;     // 메인 루프 제어
static void SigIntHandler(int) { g_running = false; } // Ctrl+C 시 종료 신호

// === main ===
int main()
{
    std::signal(SIGINT, SigIntHandler);        // SIGINT 등록(깨끗한 종료)
    ServerPacketHandler::Init();               // 패킷 핸들러 테이블 초기화

    std::cout << "[Client] Fixed profile: clients=" << kClients
        << " rooms=" << kRooms
        << " rps=" << kRps << std::endl;       // 시작 배너(실험값 확인용)

    ClientSimulator sim;                       // 시뮬레이터 인스턴스
    sim.Start();                               // IOCP/런루프 시작

    while (g_running)                          // 메인 스레드는 단순 대기
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 과도한 CPU 사용 방지
    }

    sim.Stop();                                // 워커 종료/조인
    return 0;                                  // 정상 종료
}

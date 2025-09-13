#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "ChatSession.h"
#include "DBConnectionPool.h"
#include <vector>
#include <chrono>

#include "Metrics.h" // 성능/상태 지표 모니터링용 전역 객체

// Room 객체가 생성될 때 JobQueue 초기화
Room::Room() : _jobQueue(0)
{
    _jobQueue.Start(1); // 방마다 JobQueue 스레드 하나만 돌린다 → 로그/DB 작업 전용
    // roomId는 여기선 아직 없음 → 나중에 SetId()로 지정
}

// Room 객체 소멸 시 실행
Room::~Room()
{
    _jobQueue.Stop();              // JobQueue 멈추고
    if (_logFile.is_open())        // 로그파일 열려있으면
        _logFile.close();          // 닫아준다
}

// Room ID를 지정하고, 그에 맞는 로그 파일 생성
void Room::SetId(int32 roomId) {
    _roomId = roomId;              // ID 저장
    if (_logFile.is_open()) _logFile.close(); // 이미 열려 있던 파일 닫고

    std::string filename = "Room_" + std::to_string(_roomId) + "_chat.log"; // 파일명 규칙
    _logFile.open(filename, std::ios::out | std::ios::app); // append 모드로 열기

    if (!_logFile.is_open()) {     // 실패하면 에러 출력 + Metrics 증가
        std::cerr << "Failed to open log file for room " << _roomId << std::endl;
        GMetrics.room_log_open_fail.fetch_add(1, std::memory_order_relaxed);
    }
}

// 플레이어 입장 처리
bool Room::Enter(PlayerRef player)
{
    WRITE_LOCK;                    // 쓰기 락 → 여러 스레드가 동시에 수정 못 하게
    if (_players.size() >= 100) {  // 100명 초과면 실패 처리
        GMetrics.room_enter_fail.fetch_add(1, std::memory_order_relaxed); // 실패 카운트
        return false;
    }

    _players[player->playerId] = player;   // playerId 키로 map에 등록
    player->_room = shared_from_this();    // Player가 현재 방을 가리키게

    GMetrics.room_enter_ok.fetch_add(1, std::memory_order_relaxed); // 성공 카운트
    return true;
}

// 플레이어 퇴장 처리
void Room::Leave(PlayerRef player)
{
    WRITE_LOCK;                            // 쓰기 락
    _players.erase(player->playerId);      // map에서 제거
    player->_room = nullptr;               // 플레이어가 Room 참조 해제

    GMetrics.room_leave.fetch_add(1, std::memory_order_relaxed); // 퇴장 카운트
}

// 방 안의 모든 세션에게 메시지 전송
void Room::Broadcast(SendBufferRef sendBuffer)
{
    std::vector<PacketSessionRef> targets; // 실제로 보낼 세션만 모아둘 벡터
    {
        READ_LOCK;                         // 읽기 락만 걸고
        targets.reserve(_players.size());  // 미리 공간 확보
        for (const auto& kv : _players)    // 모든 플레이어 순회
        {
            const auto& player = kv.second;
            if (player && player->ownerSession) // 세션이 살아있으면
                targets.push_back(player->ownerSession); // 전송 대상에 추가
        }
    }

    if (targets.empty()) return;           // 보낼 대상 없으면 리턴

    GMetrics.broadcast_deliveries.fetch_add(targets.size(), std::memory_order_relaxed); // 전달 개수 기록
    GMetrics.app_packets_out.fetch_add(1, std::memory_order_relaxed); // 브로드캐스트 1건 기록

    for (auto& sess : targets)             // 모아둔 세션들한테
        sess->Send(sendBuffer);            // Send 호출
}

// 자기 자신을 제외한 모든 세션에게 브로드캐스트
void Room::BroadcastWithoutSelf(SendBufferRef sendBuffer, uint64 selfId)
{
    std::vector<PacketSessionRef> targets;
    {
        READ_LOCK;
        targets.reserve(_players.size());
        for (const auto& kv : _players)
        {
            const uint64 pid = kv.first;
            const auto& player = kv.second;
            if (pid == selfId) continue;   // 본인은 제외
            if (player && player->ownerSession)
                targets.push_back(player->ownerSession);
        }
    }

    if (targets.empty()) return;

    GMetrics.broadcast_deliveries.fetch_add(targets.size(), std::memory_order_relaxed);
    GMetrics.app_packets_out.fetch_add(1, std::memory_order_relaxed);

    for (auto& sess : targets)
        sess->Send(sendBuffer);
}

// 채팅 로그를 파일에 기록
void Room::LogChat(uint64 playerId, const std::string& playerName, const std::string& message)
{
    if (!_logFile.is_open()) return;       // 파일 안 열려있으면 무시

    auto now = std::chrono::system_clock::now(); // 현재 시간
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();   // epoch 기준 ms로 변환

    _jobQueue.TryEnqueue([this, ms, playerId, playerName, message]() {
        if (_logFile.is_open()) {          // 큐 안에서 실행 → 비동기 처리
            _logFile << ms << "," << playerId << "," << playerName << "," << message << std::endl;
        }
        });

    GMetrics.chat_log_file_lines.fetch_add(1, std::memory_order_relaxed); // 라인 수 카운트
}

// 채팅 로그를 DB에 기록
void Room::LogChatToDB(uint64 playerId, const std::string& playerName, const std::string& message)
{
    _jobQueue.TryEnqueue([playerId, playerName, message]() { // JobQueue에 DB 작업 등록
        DBConnection* dbConn = GDBConnectionPool->Pop();     // 커넥션 풀에서 하나 가져오기
        if (!dbConn) {                                       // 실패 시 Metrics만 기록하고 종료
            GMetrics.conn_acquire_fail.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        GMetrics.conn_pool_pop_total.fetch_add(1, std::memory_order_relaxed); // 풀 pop 기록

        try {
            std::wstring query = L"INSERT INTO ChatLogs (PlayerId, PlayerName, Message, Timestamp) VALUES (?, ?, ?, ?)";
            if (dbConn->Prepare(query.c_str())) {            // 쿼리 준비
                GMetrics.db_query_count.fetch_add(1, std::memory_order_relaxed);

                auto wname = std::wstring(playerName.begin(), playerName.end()); // string → wstring 변환
                auto wmsg = std::wstring(message.begin(), message.end());

                SQLLEN idLen = 0, nameLen = SQL_NTS, msgLen = SQL_NTS, tsLen = 0;
                int64 ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count(); // 현재시간(ms)

                // 바인딩: 파라미터 순서대로 PlayerId, Name, Message, Timestamp
                dbConn->BindParam(1, SQL_C_SBIGINT, SQL_BIGINT, 0, (SQLPOINTER)&playerId, &idLen);
                dbConn->BindParam(2, SQL_C_WCHAR, SQL_WVARCHAR, (SQLULEN)wname.size(), (SQLPOINTER)wname.c_str(), &nameLen);
                dbConn->BindParam(3, SQL_C_WCHAR, SQL_WVARCHAR, (SQLULEN)wmsg.size(), (SQLPOINTER)wmsg.c_str(), &msgLen);
                dbConn->BindParam(4, SQL_C_SBIGINT, SQL_BIGINT, 0, (SQLPOINTER)&ts, &tsLen);

                if (dbConn->Execute()) {                     // 실행 성공 시
                    GMetrics.db_exec_ok.fetch_add(1, std::memory_order_relaxed);
                    GMetrics.chat_log_db_ok.fetch_add(1, std::memory_order_relaxed);
                }
                else {                                       // 실패 시
                    GMetrics.db_exec_fail.fetch_add(1, std::memory_order_relaxed);
                    GMetrics.chat_log_db_fail.fetch_add(1, std::memory_order_relaxed);
                }
            }
            else {                                           // Prepare 실패 시
                GMetrics.db_prepare_fail.fetch_add(1, std::memory_order_relaxed);
                GMetrics.chat_log_db_fail.fetch_add(1, std::memory_order_relaxed);
            }
        }
        catch (...) {                                        // 예외 발생 시
            GMetrics.chat_log_db_fail.fetch_add(1, std::memory_order_relaxed);
        }

        dbConn->Unbind();                                    // 파라미터 Unbind
        GMetrics.db_unbind_calls.fetch_add(1, std::memory_order_relaxed);

        GDBConnectionPool->Push(dbConn);                     // 커넥션 풀에 반환
        GMetrics.conn_pool_push_total.fetch_add(1, std::memory_order_relaxed);
        });
}

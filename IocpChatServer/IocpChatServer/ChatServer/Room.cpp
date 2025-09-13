#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "ChatSession.h"
#include "DBConnectionPool.h"
#include <vector>
#include <chrono>
#include "Log.h"

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
    }
}

// 플레이어 입장 처리
bool Room::Enter(PlayerRef player)
{
    WRITE_LOCK;                    // 쓰기 락 → 여러 스레드가 동시에 수정 못 하게
    if (_players.size() >= 100) {  // 100명 초과면 실패 처리
        return false;
    }
    _players[player->playerId] = player;   // playerId 키로 map에 등록
    player->_room = shared_from_this();    // Player가 현재 방을 가리키게

    LOG_SNAPSHOT_CTX(player->ownerSession.get(), player->playerId, _roomId,
        "Enter OK members=%zu", _players.size());
    return true;
}


// 플레이어 퇴장 처리
void Room::Leave(PlayerRef player)
{
    WRITE_LOCK;                            // 쓰기 락
    _players.erase(player->playerId);      // map에서 제거
    player->_room = nullptr;               // 플레이어가 Room 참조 해제
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

    LOG_SNAPSHOT_CTX(nullptr, 0, _roomId, "Broadcast snapshot members=%zu deliveries=%zu",
        targets.size(), targets.size());

    for (auto& sess : targets)             // 모아둔 세션들한테
        sess->Send(sendBuffer);            // Send 호출
}



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

    LOG_SNAPSHOT_CTX(nullptr, 0, _roomId,
        "BroadcastWithoutSelf snapshot members=%zu deliveries=%zu (exclude=%llu)",
        targets.size(), targets.size(), (unsigned long long)selfId);

    for (auto& sess : targets)
        sess->Send(sendBuffer);
}



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
}

void Room::LogChatToDB(uint64 playerId, const std::string& playerName, const std::string& message)
{
    _jobQueue.TryEnqueue([playerId, playerName, message]() { // JobQueue에 DB 작업 등록
        DBConnection* dbConn = GDBConnectionPool->Pop();     // 커넥션 풀에서 하나 가져오기
        if (!dbConn) {                                       // 실패 시 Metrics만 기록하고 종료
            return;
        }


        try {
            std::wstring query = L"INSERT INTO ChatLogs (PlayerId, PlayerName, Message, Timestamp) VALUES (?, ?, ?, ?)";
            if (dbConn->Prepare(query.c_str())) {            // 쿼리 준비

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

             
               
            }

        }
        catch (...) {                                        // 예외 발생 시
        }

        dbConn->Unbind();                                    // 파라미터 Unbind
        GDBConnectionPool->Push(dbConn);                     // 커넥션 풀에 반환

        });
}

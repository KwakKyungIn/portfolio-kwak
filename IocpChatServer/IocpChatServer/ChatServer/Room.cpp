#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "ChatSession.h"
#include "DBConnectionPool.h"
#include <vector>
#include <chrono>
#include "Log.h"

// Room ��ü�� ������ �� JobQueue �ʱ�ȭ
Room::Room() : _jobQueue(0)
{
    _jobQueue.Start(1); // �渶�� JobQueue ������ �ϳ��� ������ �� �α�/DB �۾� ����
    // roomId�� ���⼱ ���� ���� �� ���߿� SetId()�� ����
}

// Room ��ü �Ҹ� �� ����
Room::~Room()
{
    _jobQueue.Stop();              // JobQueue ���߰�
    if (_logFile.is_open())        // �α����� ����������
        _logFile.close();          // �ݾ��ش�
}

// Room ID�� �����ϰ�, �׿� �´� �α� ���� ����
void Room::SetId(int32 roomId) {
    _roomId = roomId;              // ID ����
    if (_logFile.is_open()) _logFile.close(); // �̹� ���� �ִ� ���� �ݰ�

    std::string filename = "Room_" + std::to_string(_roomId) + "_chat.log"; // ���ϸ� ��Ģ
    _logFile.open(filename, std::ios::out | std::ios::app); // append ���� ����

    if (!_logFile.is_open()) {     // �����ϸ� ���� ��� + Metrics ����
        std::cerr << "Failed to open log file for room " << _roomId << std::endl;
    }
}

// �÷��̾� ���� ó��
bool Room::Enter(PlayerRef player)
{
    WRITE_LOCK;                    // ���� �� �� ���� �����尡 ���ÿ� ���� �� �ϰ�
    if (_players.size() >= 100) {  // 100�� �ʰ��� ���� ó��
        return false;
    }
    _players[player->playerId] = player;   // playerId Ű�� map�� ���
    player->_room = shared_from_this();    // Player�� ���� ���� ����Ű��

    LOG_SNAPSHOT_CTX(player->ownerSession.get(), player->playerId, _roomId,
        "Enter OK members=%zu", _players.size());
    return true;
}


// �÷��̾� ���� ó��
void Room::Leave(PlayerRef player)
{
    WRITE_LOCK;                            // ���� ��
    _players.erase(player->playerId);      // map���� ����
    player->_room = nullptr;               // �÷��̾ Room ���� ����
}

// �� ���� ��� ���ǿ��� �޽��� ����
void Room::Broadcast(SendBufferRef sendBuffer)
{
    std::vector<PacketSessionRef> targets; // ������ ���� ���Ǹ� ��Ƶ� ����
    {
        READ_LOCK;                         // �б� ���� �ɰ�
        targets.reserve(_players.size());  // �̸� ���� Ȯ��
        for (const auto& kv : _players)    // ��� �÷��̾� ��ȸ
        {
            const auto& player = kv.second;
            if (player && player->ownerSession) // ������ ���������
                targets.push_back(player->ownerSession); // ���� ��� �߰�
        }
    }

    if (targets.empty()) return;           // ���� ��� ������ ����

    LOG_SNAPSHOT_CTX(nullptr, 0, _roomId, "Broadcast snapshot members=%zu deliveries=%zu",
        targets.size(), targets.size());

    for (auto& sess : targets)             // ��Ƶ� ���ǵ�����
        sess->Send(sendBuffer);            // Send ȣ��
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
            if (pid == selfId) continue;   // ������ ����
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
    if (!_logFile.is_open()) return;       // ���� �� ���������� ����

    auto now = std::chrono::system_clock::now(); // ���� �ð�
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();   // epoch ���� ms�� ��ȯ

    _jobQueue.TryEnqueue([this, ms, playerId, playerName, message]() {
        if (_logFile.is_open()) {          // ť �ȿ��� ���� �� �񵿱� ó��
            _logFile << ms << "," << playerId << "," << playerName << "," << message << std::endl;
        }
        });
}

void Room::LogChatToDB(uint64 playerId, const std::string& playerName, const std::string& message)
{
    _jobQueue.TryEnqueue([playerId, playerName, message]() { // JobQueue�� DB �۾� ���
        DBConnection* dbConn = GDBConnectionPool->Pop();     // Ŀ�ؼ� Ǯ���� �ϳ� ��������
        if (!dbConn) {                                       // ���� �� Metrics�� ����ϰ� ����
            return;
        }


        try {
            std::wstring query = L"INSERT INTO ChatLogs (PlayerId, PlayerName, Message, Timestamp) VALUES (?, ?, ?, ?)";
            if (dbConn->Prepare(query.c_str())) {            // ���� �غ�

                auto wname = std::wstring(playerName.begin(), playerName.end()); // string �� wstring ��ȯ
                auto wmsg = std::wstring(message.begin(), message.end());

                SQLLEN idLen = 0, nameLen = SQL_NTS, msgLen = SQL_NTS, tsLen = 0;
                int64 ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count(); // ����ð�(ms)

                // ���ε�: �Ķ���� ������� PlayerId, Name, Message, Timestamp
                dbConn->BindParam(1, SQL_C_SBIGINT, SQL_BIGINT, 0, (SQLPOINTER)&playerId, &idLen);
                dbConn->BindParam(2, SQL_C_WCHAR, SQL_WVARCHAR, (SQLULEN)wname.size(), (SQLPOINTER)wname.c_str(), &nameLen);
                dbConn->BindParam(3, SQL_C_WCHAR, SQL_WVARCHAR, (SQLULEN)wmsg.size(), (SQLPOINTER)wmsg.c_str(), &msgLen);
                dbConn->BindParam(4, SQL_C_SBIGINT, SQL_BIGINT, 0, (SQLPOINTER)&ts, &tsLen);

             
               
            }

        }
        catch (...) {                                        // ���� �߻� ��
        }

        dbConn->Unbind();                                    // �Ķ���� Unbind
        GDBConnectionPool->Push(dbConn);                     // Ŀ�ؼ� Ǯ�� ��ȯ

        });
}

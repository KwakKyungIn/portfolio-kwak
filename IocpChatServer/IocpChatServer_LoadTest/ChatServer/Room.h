#pragma once
#include "Protocol.pb.h"
#include "Session.h"
#include "Player.h"
#include "JobQueue.h"
#include <map>
#include <memory>
#include <fstream>

class Room;
using RoomRef = std::shared_ptr<Room>;

// ä�ù�(Room) Ŭ����
//  - �÷��̾� ���� (����/����)
//  - �޽��� ��ε�ĳ��Ʈ
//  - ä�� �α� ����/DB ���
class Room : public std::enable_shared_from_this<Room>
{
public:
    Room();
    ~Room();

    bool Enter(PlayerRef player);                      // �÷��̾� ����
    void Leave(PlayerRef player);                      // �÷��̾� ����

    void Broadcast(SendBufferRef sendBuffer);          // ��ü ��ε�ĳ��Ʈ
    void BroadcastWithoutSelf(SendBufferRef sendBuffer,
        uint64 selfId);          // ���� ���� ��ε�ĳ��Ʈ

    void SetId(int32 roomId);                          // Room ID ���� + �α� ���� ����
    void SetName(const std::string& name) { _roomName = name; }
    void SetType(Protocol::RoomType type) { _roomType = type; }

    int32 GetId() { return _roomId; }
    const std::string& GetName() { return _roomName; }
    Protocol::RoomType GetType() { return _roomType; }

    void LogChat(uint64 playerId,
        const std::string& playerName,
        const std::string& message);          // ���� �α�
    void LogChatToDB(uint64 playerId,
        const std::string& playerName,
        const std::string& message);      // DB �α�

    const std::map<uint64, PlayerRef>& GetPlayers() { return _players; }

    size_t PendingJobs() const { return _jobQueue.Size(); }

private:
    USE_LOCK;                                          // RW Lock (��Ƽ������ ���)

    int32                   _roomId = 0;              // Room ���� ID
    std::string             _roomName;                // Room �̸�
    Protocol::RoomType      _roomType;                // Room Ÿ�� (Public, Private ��)
    std::map<uint64, PlayerRef> _players;             // ���� Room�� ���� �÷��̾��

    JobQueue                _jobQueue;                // �α�/DB ��Ͽ� JobQueue
    std::ofstream           _logFile;                 // ä�� �α� ���� �ڵ�
};

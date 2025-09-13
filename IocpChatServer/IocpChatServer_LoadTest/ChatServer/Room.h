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

// 채팅방(Room) 클래스
//  - 플레이어 관리 (입장/퇴장)
//  - 메시지 브로드캐스트
//  - 채팅 로그 파일/DB 기록
class Room : public std::enable_shared_from_this<Room>
{
public:
    Room();
    ~Room();

    bool Enter(PlayerRef player);                      // 플레이어 입장
    void Leave(PlayerRef player);                      // 플레이어 퇴장

    void Broadcast(SendBufferRef sendBuffer);          // 전체 브로드캐스트
    void BroadcastWithoutSelf(SendBufferRef sendBuffer,
        uint64 selfId);          // 본인 제외 브로드캐스트

    void SetId(int32 roomId);                          // Room ID 세팅 + 로그 파일 오픈
    void SetName(const std::string& name) { _roomName = name; }
    void SetType(Protocol::RoomType type) { _roomType = type; }

    int32 GetId() { return _roomId; }
    const std::string& GetName() { return _roomName; }
    Protocol::RoomType GetType() { return _roomType; }

    void LogChat(uint64 playerId,
        const std::string& playerName,
        const std::string& message);          // 파일 로그
    void LogChatToDB(uint64 playerId,
        const std::string& playerName,
        const std::string& message);      // DB 로그

    const std::map<uint64, PlayerRef>& GetPlayers() { return _players; }

    size_t PendingJobs() const { return _jobQueue.Size(); }

private:
    USE_LOCK;                                          // RW Lock (멀티스레드 대비)

    int32                   _roomId = 0;              // Room 고유 ID
    std::string             _roomName;                // Room 이름
    Protocol::RoomType      _roomType;                // Room 타입 (Public, Private 등)
    std::map<uint64, PlayerRef> _players;             // 현재 Room에 들어온 플레이어들

    JobQueue                _jobQueue;                // 로그/DB 기록용 JobQueue
    std::ofstream           _logFile;                 // 채팅 로그 파일 핸들
};

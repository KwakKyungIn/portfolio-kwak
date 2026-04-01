#include "pch.h"
#include "RoomManager.h"
#include "DataManager.h"

std::shared_ptr<RoomManager> GRoomManager = nullptr;

// 방을 찾거나, 없으면 새로 만드는 함수
// 멀티스레드 환경이라 동시에 여러 유저가 들어올 때를 대비해야 함
std::shared_ptr<GameRoom> RoomManager::GetOrCreateRoom(int32 channelId, int32 mapId, int64 instanceId)
{
    DataManager* dm = DataManager::Instance();
    // 맵 ID가 유효하지 않으면 기본 맵으로 보냄 (방어 코드)
    if (dm && dm->IsValidMapId(mapId) == false)
        mapId = dm->GetDefaultMapId();

    RoomKey key{ channelId, mapId, instanceId }; // instanceId 포함해서 키 생성

    // [1차 시도] 읽기 락(Read Lock)만 걸고 검색
    // 대부분의 경우 방은 이미 존재할 것이므로, 굳이 처음부터 무거운 쓰기 락을 걸 필요가 없음
    // 성능 최적화를 위한 구조
    {
        READ_LOCK;
        auto it = _rooms.find(key);
        if (it != _rooms.end())
            return it->second;
    }

    // [2차 시도] 방이 없어서 생성해야 함 -> 쓰기 락(Write Lock)
    WRITE_LOCK;
    // 락을 교체하는 사이에 다른 스레드가 방을 만들었을 수도 있으니 한 번 더 체크 (Double-Check)
    auto it = _rooms.find(key);
    if (it != _rooms.end())
        return it->second;

    // 진짜 없으니까 새로 만듦
    const MapConfig* cfg = (dm ? dm->GetMapConfig(mapId) : nullptr);

    // 맵 크기 정보가 없으면 기본값 사용
    const int32 sizeX = cfg ? cfg->sizeX : 100;
    const int32 sizeY = cfg ? cfg->sizeY : 100;
    const int32 zoneSize = cfg ? cfg->zoneSize : 10;

    auto room = MakeShared<GameRoom>();
    room->Init(channelId, mapId, sizeX, sizeY, zoneSize);

    room->SetInstanceId(instanceId);

    _rooms[key] = room;
    return room;
}

// 단순히 방이 있는지 조회만 하는 함수
std::shared_ptr<GameRoom> RoomManager::FindRoom(int32 channelId, int32 mapId, int64 instanceId)
{
    DataManager* dm = DataManager::Instance();
    if (dm && dm->IsValidMapId(mapId) == false)
        mapId = dm->GetDefaultMapId();

    RoomKey key{ channelId, mapId, instanceId };

    // 조회만 하니까 Read Lock으로 충분함
    READ_LOCK;
    auto it = _rooms.find(key);
    if (it == _rooms.end()) return nullptr;
    return it->second;
}

// 로비(대기실) 관리
// 로비는 채널당 하나만 존재한다고 가정
std::shared_ptr<LobbyRoom> RoomManager::GetOrCreateLobby(int32 channelId)
{
    // 여기도 마찬가지로 읽기 락 먼저 시도해서 경합을 줄임
    {
        READ_LOCK;
        auto it = _lobbies.find(channelId);
        if (it != _lobbies.end())
            return it->second;
    }

    WRITE_LOCK;
    auto it = _lobbies.find(channelId);
    if (it != _lobbies.end())
        return it->second;

    auto lobby = MakeShared<LobbyRoom>();
    lobby->Init(channelId);
    _lobbies[channelId] = lobby;
    return lobby;
}


#include <Windows.h>

// 메인 스레드에서 주기적으로 호출해주는 업데이트 함수
void RoomManager::UpdateAll()
{
    Vector<std::shared_ptr<GameRoom>> roomsCopy;

    // 락을 오래 잡고 있으면 안 되니까, 방 목록만 복사해오고 락은 바로 품
    {
        READ_LOCK;
        for (auto& kv : _rooms)
            roomsCopy.push_back(kv.second);
    }

    // 실제 업데이트 로직은 여기서 돌리는 게 아니라, 각 방의 JobQueue에 밀어넣음
    // 그래야 병렬 처리가 가능해짐
    for (auto& room : roomsCopy)
    {
        if (!room) continue;
        room->Push([room]()
            {
                room->Update();
            });
    }

    // 빈 방 정리 (인스턴스 던전 등)
    // 매 프레임 할 필요는 없고 적당히 시간 체크해서 호출
    const uint64 nowMs = ::GetTickCount64();
    PurgeInstanceRooms(nowMs);
}

// 사용하지 않는 인스턴스 룸 정리
// 메모리 누수를 막기 위해 주기적으로 청소해줌
void RoomManager::PurgeInstanceRooms(uint64 nowMs)
{
    Vector<RoomKey> eraseKeys;

    // 1단계: 지울 대상을 선별 (Read Lock)
    {
        READ_LOCK;
        for (auto& kv : _rooms)
        {
            const RoomKey& key = kv.first;
            const auto& room = kv.second;
            if (!room) continue;

            // 일반 필드(World)는 사람이 없어도 유지하고, 인스턴스 던전만 삭제 대상
            if (key.instanceId == 0)
                continue;

            // 방 내부적으로 "나 이제 꺼져도 됨?" 체크
            if (room->ShouldPurge(nowMs))
                eraseKeys.push_back(key);
        }
    }

    if (eraseKeys.empty())
        return;

    // 2단계: 실제 삭제 (Write Lock)
    WRITE_LOCK;
    for (const RoomKey& key : eraseKeys)
    {
        auto it = _rooms.find(key);
        if (it == _rooms.end())
            continue;

        auto room = it->second;
        // 락 풀린 사이에 누가 들어왔을 수도 있으니 다시 확인
        if (room && room->ShouldPurge(nowMs))
        {
            _rooms.erase(it);
            printf("[RoomManager] Purged instance room: ch=%d map=%d inst=%lld\n",
                key.channelId, key.mapId, key.instanceId);
        }
    }
}
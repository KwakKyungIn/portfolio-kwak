#pragma once
#include "Protocol.pb.h"
#include "Session.h"
#include "Room.h"

class Player
{
public:
    uint64          playerId = 0;        // DB/서버에서 부여되는 고유 플레이어 ID
    string          name;                // 플레이어 이름
    ChatSessionRef  ownerSession;        // 이 Player를 소유한 세션 (네트워크 연결)

public:
    // 현재 플레이어가 속한 Room.
    RoomRef _room = nullptr;
};

using PlayerRef = shared_ptr<Player>;

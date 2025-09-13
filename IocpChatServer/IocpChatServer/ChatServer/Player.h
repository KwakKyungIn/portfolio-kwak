#pragma once
#include "Protocol.pb.h"
#include "Session.h"
#include "Room.h"

class Player
{
public:
    uint64          playerId = 0;        // DB/�������� �ο��Ǵ� ���� �÷��̾� ID
    string          name;                // �÷��̾� �̸�
    ChatSessionRef  ownerSession;        // �� Player�� ������ ���� (��Ʈ��ũ ����)

public:
    // ���� �÷��̾ ���� Room.
    RoomRef _room = nullptr;
};

using PlayerRef = shared_ptr<Player>;

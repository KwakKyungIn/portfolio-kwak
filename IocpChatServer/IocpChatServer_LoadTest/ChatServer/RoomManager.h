#pragma once
#include <map>          // [STL] map �����̳� ��� (roomId �� RoomRef ����)
#include <memory>       // [����Ʈ ������] shared_ptr ���
#include "Room.h"       // Room Ŭ���� ����

// Forward declaration (���� ����)
// Room Ŭ������ �Ʒ����� shared_ptr�� ������ �ϱ� ������, ���⼭ �̸� ��������
class Room;

// �� ��ü�� ����Ű�� ����Ʈ ������ Ÿ�� ����
using RoomRef = std::shared_ptr<Room>;

// ���� �����ϴ� �Ŵ��� Ŭ����
// - �� ����(AddRoom)
// - �� ã��(FindRoom)
// - �� ����(RemoveRoom)
// ������ �����ϴ� Ŭ����
class RoomManager
{
public:
    // ���ο� Room�� ����ϰ� ���� ID�� �ο�
    int32 AddRoom(RoomRef room);

    // Ư�� roomId�� Room�� ã�� ��ȯ (������ nullptr)
    RoomRef FindRoom(int32 roomId);

    // roomId�� �ش��ϴ� Room�� ����
    void RemoveRoom(int32 roomId);

private:
    USE_LOCK;   // ��Ƽ������ ȯ�� ��ȣ�� �� (READ/WRITE_LOCK ��ũ�ο��� ���)
    int32 _nextRoomId = 1;               // ������ �ο��� roomId (1���� ����)
    std::map<int32, RoomRef> _rooms;     // ���� �����ϴ� ��� Room�� ���� (roomId �� RoomRef)
};

// �������� �ϳ��� �����ϴ� RoomManager ��ü ����
extern RoomManager GRoomManager;

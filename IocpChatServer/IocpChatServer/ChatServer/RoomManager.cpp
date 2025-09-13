#include "pch.h"                  // PCH: ���� ��� �̸� �������ؼ� ���� �ð� ����
#include "RoomManager.h"          // RoomManager ���� �о����
#include "Log.h"

RoomManager GRoomManager;         // ���� �̱��� �Ŵ��� �ν��Ͻ�(������ �������� ���)

/*-----------------------
    AddRoom
  - �� Room�� ���� id �ο�
  - �ʿ� ���
  - �� ���� ��Ʈ�� ���� + ��ũ ����
------------------------*/
int32 RoomManager::AddRoom(RoomRef room)
{
    WRITE_LOCK;                   // ���� ���(���ÿ� �߰�/���� �浹 ����)
    int32 newId = _nextRoomId++;  // �� �� ��ȣ �߱�(��ġ������ ���� id �غ�)
    _rooms[newId] = room;         // id �� RoomRef ���� ����

    LOG_SNAPSHOT_CTX(nullptr, 0, newId, "RoomManager.AddRoom WRITE �� roomId=%d", newId);
    return newId;                 // ȣ���ڿ��� �� �� id ��ȯ
}

/*-----------------------
    FindRoom
  - id�� �� �˻�
  - ������ nullptr
------------------------*/
RoomRef RoomManager::FindRoom(int32 roomId)
{
    READ_LOCK;                    // �б� ���(���� ��ȸ ���, ����ʹ� ��Ÿ)
    auto it = _rooms.find(roomId);// map���� �ش� Ű Ž��
    if (it != _rooms.end())       // ã������
    {
        return it->second;        // RoomRef ��ȯ(���� ������)
    }
    return nullptr;               // ������ �� ������
}

/*-----------------------
    RemoveRoom
  - id�� �� ����
  - ���� �� ��Ʈ�� ����
------------------------*/
void RoomManager::RemoveRoom(int32 roomId)
{
    WRITE_LOCK;                   // ���� ���(���� �� ���̽� ����)
    auto erased = _rooms.erase(roomId); // Ű�� ������ 1, ������ 0

    LOG_SNAPSHOT_CTX(nullptr, 0, roomId, "RoomManager.RemoveRoom roomId=%d", roomId);
}

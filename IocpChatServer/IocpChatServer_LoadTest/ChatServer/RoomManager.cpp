#include "pch.h"                  // PCH: ���� ��� �̸� �������ؼ� ���� �ð� ����
#include "RoomManager.h"          // RoomManager ���� �о����
#include "Metrics.h"              // [METRICS] ���� ���� ����(GMetrics) ���

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

    // [METRICS] ���� �� �� +1 (������ ����, now�� ���� �� ��)
    uint32_t now = GMetrics.rooms_gauge.fetch_add(1, std::memory_order_relaxed) + 1;

    // [METRICS] 1�� ���� ���� �Ǵ� ��(��ũ) ����: now�� prev���� ũ�� CAS�� ��ü �õ�
    uint32_t prev = GMetrics.rooms_peak.load(std::memory_order_relaxed);
    while (now > prev && !GMetrics.rooms_peak.compare_exchange_weak(prev, now, std::memory_order_relaxed)) {}

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
    if (erased > 0) {             // ������ �������ٸ鸸
        // [METRICS] ���� �� �� -1 (��ũ�� 1�� ƽ���� ���� ����/����)
        GMetrics.rooms_gauge.fetch_sub(1, std::memory_order_relaxed);
    }
}

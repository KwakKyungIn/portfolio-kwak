#include "pch.h"                  // PCH: 공용 헤더 미리 컴파일해서 빌드 시간 단축
#include "RoomManager.h"          // RoomManager 선언 읽어오기
#include "Metrics.h"              // [METRICS] 전역 계측 변수(GMetrics) 사용

RoomManager GRoomManager;         // 전역 싱글톤 매니저 인스턴스(간단히 전역으로 운용)

/*-----------------------
    AddRoom
  - 새 Room에 고유 id 부여
  - 맵에 등록
  - 방 개수 메트릭 증가 + 피크 갱신
------------------------*/
int32 RoomManager::AddRoom(RoomRef room)
{
    WRITE_LOCK;                   // 쓰기 잠금(동시에 추가/삭제 충돌 방지)
    int32 newId = _nextRoomId++;  // 새 방 번호 발급(후치증가로 다음 id 준비)
    _rooms[newId] = room;         // id → RoomRef 매핑 저장

    // [METRICS] 현재 방 수 +1 (원자적 증가, now는 증가 후 값)
    uint32_t now = GMetrics.rooms_gauge.fetch_add(1, std::memory_order_relaxed) + 1;

    // [METRICS] 1초 동안 가장 컸던 값(피크) 갱신: now가 prev보다 크면 CAS로 교체 시도
    uint32_t prev = GMetrics.rooms_peak.load(std::memory_order_relaxed);
    while (now > prev && !GMetrics.rooms_peak.compare_exchange_weak(prev, now, std::memory_order_relaxed)) {}

    return newId;                 // 호출자에게 새 방 id 반환
}

/*-----------------------
    FindRoom
  - id로 방 검색
  - 없으면 nullptr
------------------------*/
RoomRef RoomManager::FindRoom(int32 roomId)
{
    READ_LOCK;                    // 읽기 잠금(동시 조회 허용, 쓰기와는 배타)
    auto it = _rooms.find(roomId);// map에서 해당 키 탐색
    if (it != _rooms.end())       // 찾았으면
    {
        return it->second;        // RoomRef 반환(공유 소유권)
    }
    return nullptr;               // 없으면 빈 포인터
}

/*-----------------------
    RemoveRoom
  - id로 방 제거
  - 성공 시 메트릭 감소
------------------------*/
void RoomManager::RemoveRoom(int32 roomId)
{
    WRITE_LOCK;                   // 쓰기 잠금(삭제 중 레이스 방지)
    auto erased = _rooms.erase(roomId); // 키가 있으면 1, 없으면 0
    if (erased > 0) {             // 실제로 지워졌다면만
        // [METRICS] 현재 방 수 -1 (피크는 1초 틱에서 따로 리셋/관리)
        GMetrics.rooms_gauge.fetch_sub(1, std::memory_order_relaxed);
    }
}

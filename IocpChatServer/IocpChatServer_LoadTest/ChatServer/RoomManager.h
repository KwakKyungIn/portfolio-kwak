#pragma once
#include <map>          // [STL] map 컨테이너 사용 (roomId → RoomRef 매핑)
#include <memory>       // [스마트 포인터] shared_ptr 사용
#include "Room.h"       // Room 클래스 참조

// Forward declaration (전방 선언)
// Room 클래스는 아래에서 shared_ptr로 참조만 하기 때문에, 여기서 미리 선언해줌
class Room;

// 방 객체를 가리키는 스마트 포인터 타입 정의
using RoomRef = std::shared_ptr<Room>;

// 방을 관리하는 매니저 클래스
// - 방 생성(AddRoom)
// - 방 찾기(FindRoom)
// - 방 삭제(RemoveRoom)
// 역할을 전담하는 클래스
class RoomManager
{
public:
    // 새로운 Room을 등록하고 고유 ID를 부여
    int32 AddRoom(RoomRef room);

    // 특정 roomId로 Room을 찾아 반환 (없으면 nullptr)
    RoomRef FindRoom(int32 roomId);

    // roomId에 해당하는 Room을 삭제
    void RemoveRoom(int32 roomId);

private:
    USE_LOCK;   // 멀티스레드 환경 보호용 락 (READ/WRITE_LOCK 매크로에서 사용)
    int32 _nextRoomId = 1;               // 다음에 부여할 roomId (1부터 시작)
    std::map<int32, RoomRef> _rooms;     // 현재 존재하는 모든 Room을 관리 (roomId → RoomRef)
};

// 전역으로 하나만 존재하는 RoomManager 객체 선언
extern RoomManager GRoomManager;

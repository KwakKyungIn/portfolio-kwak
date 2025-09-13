#include "pch.h"
#include "ServerPacketHandler.h"
#include "ServerSession.h" // 새로 만든 헤더 포함

// [개요] 클라 전용 패킷 처리부
// - GPacketHandler: 패킷 ID → 처리 함수 테이블 (ServerPacketHandler::Init에서 채움)
// - 아래 핸들러들은 protobuf로 파싱된 결과(…RES/…NTF)를 받아서
//   클라 상태(ServerSession) 갱신 + 간단한 로그 출력 역할

PacketHandlerFunc GPacketHandler[UINT16_MAX];

// ===================== INVALID =====================
// 서버가 보낸 패킷 ID가 매칭되지 않을 때 (테이블 초기화 실패, 잘못된 데이터 등)
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
    // TODO: log
    return false;
}

// ===================== LOGIN =====================
// 로그인 응답: 상태 체크 후 클라 측 세션 상태 갱신
bool Handle_S_LOGIN_RES(PacketSessionRef& session, Protocol::S_LOGIN_RES& pkt)
{
    auto cli = std::dynamic_pointer_cast<ServerSession>(session);
    if (!cli) return false;

    if (pkt.status() == Protocol::CONNECT_OK)
    {
        cli->isLoggedIn = true; // 성공 시 로그인 상태 on
        std::cout << "[" << cli->name << "] Login Success! Player ID: "
            << pkt.playerid() << " / Server Message: " << pkt.reason() << std::endl;
    }
    else
    {
        // 실패 사유는 서버가 내려준 reason에 들어있음
        std::cout << "[" << cli->name << "] Login Fail! Reason: " << pkt.reason() << std::endl;
    }
    return true;
}

// ===================== CREATE ROOM =====================
// 방 생성 응답: 성공 시 로컬 상태(참여 여부/roomId) 갱신 + 멤버 목록 출력
bool Handle_S_CREATE_ROOM_RES(PacketSessionRef& session, Protocol::S_CREATE_ROOM_RES& pkt)
{
    auto cli = std::dynamic_pointer_cast<ServerSession>(session);
    if (!cli) return false;

    if (pkt.success())
    {
        cli->isInRoom = true;
        cli->roomId = static_cast<int32>(pkt.room().roomid());

        std::cout << "[" << cli->name << "] New Room Success -> Room ID: "
            << pkt.room().roomid() << " / Name: " << pkt.room().roomname() << std::endl;

        // 서버가 내려준 현재 멤버 스냅샷 출력 (디버깅/테스트에 유용)
        for (const auto& member : pkt.room().members())
        {
            std::cout << " - " << member.name()
                << " (ID: " << member.playerid() << ")" << std::endl;
        }
    }
    else
    {
        std::cout << "[" << cli->name << "] New Room Fail -> " << pkt.reason() << std::endl;
    }
    return true;
}

// ===================== JOIN ROOM =====================
// 방 입장 응답: 성공 시 참여 플래그/roomId 업데이트
bool Handle_S_JOIN_ROOM_RES(PacketSessionRef& session, Protocol::S_JOIN_ROOM_RES& pkt)
{
    auto cli = std::dynamic_pointer_cast<ServerSession>(session);
    if (!cli) return false;

    if (pkt.success())
    {
        cli->isInRoom = true;
        cli->roomId = static_cast<int32>(pkt.room().roomid());

        std::cout << "[" << cli->name << "] Room Enter Success -> Room "
            << pkt.room().roomid() << " (" << pkt.room().roomname() << ")" << std::endl;
    }
    else
    {
        std::cout << "[" << cli->name << "] Room Enter fail -> " << pkt.reason() << std::endl;
    }
    return true;
}

// 누군가 입장했다는 알림(내가 아닌 다른 유저에 대한 이벤트)
bool Handle_S_JOIN_ROOM_NTF(PacketSessionRef& session, Protocol::S_JOIN_ROOM_NTF& pkt)
{
    std::cout << pkt.name() << " has entered!" << std::endl;
    return true;
}

// ===================== LEAVE ROOM =====================
// 방 나가기 응답: 성공 시 참여 상태 초기화
bool Handle_S_LEAVE_ROOM_ACK(PacketSessionRef& session, Protocol::S_LEAVE_ROOM_ACK& pkt)
{
    auto cli = std::dynamic_pointer_cast<ServerSession>(session);
    if (!cli) return false;

    if (pkt.success())
    {
        cli->isInRoom = false;
        cli->roomId = -1;
        std::cout << "[" << cli->name << "] Leave Room Success" << std::endl;
    }
    else
    {
        std::cout << "[" << cli->name << "] Leave Room Fail" << std::endl;
    }
    return true;
}

// ===================== ROOM CHAT =====================
// 방 채팅 알림: 현재는 출력 주석 처리 (원하면 UI/로그로 연결)
bool Handle_S_ROOM_CHAT_NTF(PacketSessionRef& session, Protocol::S_ROOM_CHAT_NTF& pkt)
{
    const auto& chat = pkt.chat();
    /*std::cout << "[Room " << pkt.roomid() << "] "
        << "<" << chat.senderid() << "> " << chat.message()
        << " (msgId=" << chat.messageid()
        << ", ts=" << chat.timestamp() << ")" << std::endl;*/
    return true;
}

// ===================== 기타 =====================
// 필요 시 확장 포인트(프레즌스/레이트리밋/관리자 커맨드 응답 등)
bool Handle_S_PRESENCE_NTF(PacketSessionRef& session, Protocol::S_PRESENCE_NTF& pkt) { return false; }
bool Handle_S_RATE_LIMIT_NTF(PacketSessionRef& session, Protocol::S_RATE_LIMIT_NTF& pkt) { return false; }
bool Handle_S_ADMIN_COMMAND_RES(PacketSessionRef& session, Protocol::S_ADMIN_COMMAND_RES& pkt) { return false; }

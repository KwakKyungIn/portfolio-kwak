#include "pch.h"
#include "ServerPacketHandler.h"


PacketHandlerFunc GPacketHandler[UINT16_MAX];
extern std::atomic<bool> g_isLoggedIn;
extern std::atomic<bool> g_isInRoom;
extern PacketSessionRef g_session;

// ======================================================================
// 직접 컨텐츠 작업자 (클라가 "서버 → 클라" 패킷을 처리하는 영역)
// ======================================================================

// ===================== INVALID =====================
// 서버가 보낸 패킷 ID가 매칭되지 않을 때 (테이블 초기화 실패, 잘못된 데이터 등)
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);  // 앞 4바이트 [size,id] 해석
	// TODO : Log (패킷 ID 범위/무결성 체크 후 기록)
	return false;                                                     // 처리 실패로 반환 (디폴트)
}


// ===================== LOGIN =====================
// 로그인 응답: 상태 체크 후 클라 전역 플래그 갱신
bool Handle_S_LOGIN_RES(PacketSessionRef& session, Protocol::S_LOGIN_RES& pkt)
{
	if (pkt.status() == Protocol::CONNECT_OK)
	{
		g_isLoggedIn = true; // 성공 시 로그인 상태 on

		std::cout << "\n로그인 성공! Player ID: " << pkt.playerid() << std::endl;
		std::cout << "서버 메시지: " << pkt.reason() << std::endl;
	}
	else
	{
		// 실패 사유는 서버가 내려준 reason에 들어있음
		std::cout << "\n로그인 실패! 이유: " << pkt.reason() << std::endl;
	}

	return true;
}

// 누군가 입장했다는 알림(내가 아닌 다른 유저에 대한 이벤트)
bool Handle_S_JOIN_ROOM_NTF(PacketSessionRef& session, Protocol::S_JOIN_ROOM_NTF& pkt)
{
	std::cout << pkt.name() << " 님이 방에 들어왔습니다!" << std::endl;
	return true;
}


// ===================== CREATE ROOM =====================
// 방 생성 응답: 성공 시 전역 참여 플래그 갱신 + 멤버 목록 출력
bool Handle_S_CREATE_ROOM_RES(PacketSessionRef& session, Protocol::S_CREATE_ROOM_RES& pkt)
{
	if (pkt.success())
	{
		const Protocol::RoomInfo& roomInfo = pkt.room();
		g_isInRoom = true; // 방 생성 성공 시 입장 상태 on

		std::cout << "\n==============================================" << std::endl;
		std::cout << "Room created successfully!\nRoom ID: " << roomInfo.roomid()
			<< ", Room Name: " << roomInfo.roomname() << std::endl;
		std::cout << "채팅을 시작하세요! (나가려면 /exit 입력)" << std::endl;
		std::cout << "--- 현재 접속자 ---" << std::endl;

		// 서버가 내려준 현재 멤버 스냅샷 출력 (디버깅/테스트에 유용)
		for (const auto& member : roomInfo.members())
		{
			std::cout << "- " << member.name() << " (ID: " << member.playerid() << ")" << std::endl;
		}
		std::cout << "==============================================" << std::endl;
	}
	else
	{
		std::cout << "Failed to create room. Reason: " << pkt.reason() << std::endl;
	}
	return true;
}


// ===================== JOIN ROOM =====================
// 방 입장 응답: 성공 시 전역 참여 플래그 갱신 + 멤버 목록 출력
bool Handle_S_JOIN_ROOM_RES(PacketSessionRef& session, Protocol::S_JOIN_ROOM_RES& pkt)
{
	if (pkt.success())
	{
		g_isInRoom = true; // 성공 시 참여 상태 on

		const Protocol::RoomInfo& roomInfo = pkt.room();
		std::cout << "\n==============================================" << std::endl;
		std::cout << "[" << roomInfo.roomname() << "] 방 (ID: " << roomInfo.roomid() << ")에 입장했습니다." << std::endl;
		std::cout << "채팅을 시작하세요! (나가려면 /exit 입력)" << std::endl;
		std::cout << "--- 현재 접속자 ---" << std::endl;

		for (const auto& member : roomInfo.members())
		{
			std::cout << "- " << member.name() << " (ID: " << member.playerid() << ")" << std::endl;
		}
		std::cout << "==============================================" << std::endl;
	}
	else
	{
		std::cout << "\n방 참가 실패! 이유: " << pkt.reason() << std::endl;
	}

	return true;
}

// ===================== ROOM CHAT =====================
// 방 채팅 알림: 발신자가 0이면 시스템 메시지, 아니면 일반 플레이어 메시지
bool Handle_S_ROOM_CHAT_NTF(PacketSessionRef& session, Protocol::S_ROOM_CHAT_NTF& pkt)
{
	int64 roomId = pkt.roomid();
	const Protocol::ChatMessage& chatMsg = pkt.chat();

	uint64 messageId = chatMsg.messageid();
	int64 senderId = chatMsg.senderid();
	const std::string& message = chatMsg.message();
	int64 timestamp = chatMsg.timestamp();

	if (senderId == 0) // 시스템 발신자
	{
		std::cout << message << std::endl;
		return true;
	}
	std::cout << "[Room " << roomId << "] "
		<< "Player(" << senderId << "): "
		<< message
		<< std::endl;
	return true;
}

// ===================== LEAVE ROOM =====================
// 방 나가기 응답: 성공 시 전역 참여 플래그 초기화
bool Handle_S_LEAVE_ROOM_ACK(PacketSessionRef& session, Protocol::S_LEAVE_ROOM_ACK& pkt)
{
	if (pkt.success())
	{
		g_isInRoom = false; // 방 나가기 성공 시 플래그 변경
		std::cout << "\n방에서 성공적으로 나갔습니다." << std::endl;
		std::cout << "메인 메뉴로 돌아갑니다." << std::endl;
	}
	else
	{
		std::cout << "\n방 나가기 실패! " << std::endl;
	}
	return true;
}


// ===================== 기타 =====================
// 필요 시 확장 포인트(프레즌스/레이트리밋/관리자 커맨드 응답 등)
bool Handle_S_PRESENCE_NTF(PacketSessionRef& session, Protocol::S_PRESENCE_NTF& pkt) { return false; }
bool Handle_S_RATE_LIMIT_NTF(PacketSessionRef& session, Protocol::S_RATE_LIMIT_NTF& pkt) { return false; }
bool Handle_S_ADMIN_COMMAND_RES(PacketSessionRef& session, Protocol::S_ADMIN_COMMAND_RES& pkt) { return false; }
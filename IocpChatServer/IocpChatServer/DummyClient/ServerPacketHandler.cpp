#include "pch.h"
#include "ServerPacketHandler.h"


PacketHandlerFunc GPacketHandler[UINT16_MAX];
extern std::atomic<bool> g_isLoggedIn;
extern std::atomic<bool> g_isInRoom;
extern PacketSessionRef g_session;

// ======================================================================
// ���� ������ �۾��� (Ŭ�� "���� �� Ŭ��" ��Ŷ�� ó���ϴ� ����)
// ======================================================================

// ===================== INVALID =====================
// ������ ���� ��Ŷ ID�� ��Ī���� ���� �� (���̺� �ʱ�ȭ ����, �߸��� ������ ��)
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);  // �� 4����Ʈ [size,id] �ؼ�
	// TODO : Log (��Ŷ ID ����/���Ἲ üũ �� ���)
	return false;                                                     // ó�� ���з� ��ȯ (����Ʈ)
}


// ===================== LOGIN =====================
// �α��� ����: ���� üũ �� Ŭ�� ���� �÷��� ����
bool Handle_S_LOGIN_RES(PacketSessionRef& session, Protocol::S_LOGIN_RES& pkt)
{
	if (pkt.status() == Protocol::CONNECT_OK)
	{
		g_isLoggedIn = true; // ���� �� �α��� ���� on

		std::cout << "\n�α��� ����! Player ID: " << pkt.playerid() << std::endl;
		std::cout << "���� �޽���: " << pkt.reason() << std::endl;
	}
	else
	{
		// ���� ������ ������ ������ reason�� �������
		std::cout << "\n�α��� ����! ����: " << pkt.reason() << std::endl;
	}

	return true;
}

// ������ �����ߴٴ� �˸�(���� �ƴ� �ٸ� ������ ���� �̺�Ʈ)
bool Handle_S_JOIN_ROOM_NTF(PacketSessionRef& session, Protocol::S_JOIN_ROOM_NTF& pkt)
{
	std::cout << pkt.name() << " ���� �濡 ���Խ��ϴ�!" << std::endl;
	return true;
}


// ===================== CREATE ROOM =====================
// �� ���� ����: ���� �� ���� ���� �÷��� ���� + ��� ��� ���
bool Handle_S_CREATE_ROOM_RES(PacketSessionRef& session, Protocol::S_CREATE_ROOM_RES& pkt)
{
	if (pkt.success())
	{
		const Protocol::RoomInfo& roomInfo = pkt.room();
		g_isInRoom = true; // �� ���� ���� �� ���� ���� on

		std::cout << "\n==============================================" << std::endl;
		std::cout << "Room created successfully!\nRoom ID: " << roomInfo.roomid()
			<< ", Room Name: " << roomInfo.roomname() << std::endl;
		std::cout << "ä���� �����ϼ���! (�������� /exit �Է�)" << std::endl;
		std::cout << "--- ���� ������ ---" << std::endl;

		// ������ ������ ���� ��� ������ ��� (�����/�׽�Ʈ�� ����)
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
// �� ���� ����: ���� �� ���� ���� �÷��� ���� + ��� ��� ���
bool Handle_S_JOIN_ROOM_RES(PacketSessionRef& session, Protocol::S_JOIN_ROOM_RES& pkt)
{
	if (pkt.success())
	{
		g_isInRoom = true; // ���� �� ���� ���� on

		const Protocol::RoomInfo& roomInfo = pkt.room();
		std::cout << "\n==============================================" << std::endl;
		std::cout << "[" << roomInfo.roomname() << "] �� (ID: " << roomInfo.roomid() << ")�� �����߽��ϴ�." << std::endl;
		std::cout << "ä���� �����ϼ���! (�������� /exit �Է�)" << std::endl;
		std::cout << "--- ���� ������ ---" << std::endl;

		for (const auto& member : roomInfo.members())
		{
			std::cout << "- " << member.name() << " (ID: " << member.playerid() << ")" << std::endl;
		}
		std::cout << "==============================================" << std::endl;
	}
	else
	{
		std::cout << "\n�� ���� ����! ����: " << pkt.reason() << std::endl;
	}

	return true;
}

// ===================== ROOM CHAT =====================
// �� ä�� �˸�: �߽��ڰ� 0�̸� �ý��� �޽���, �ƴϸ� �Ϲ� �÷��̾� �޽���
bool Handle_S_ROOM_CHAT_NTF(PacketSessionRef& session, Protocol::S_ROOM_CHAT_NTF& pkt)
{
	int64 roomId = pkt.roomid();
	const Protocol::ChatMessage& chatMsg = pkt.chat();

	uint64 messageId = chatMsg.messageid();
	int64 senderId = chatMsg.senderid();
	const std::string& message = chatMsg.message();
	int64 timestamp = chatMsg.timestamp();

	if (senderId == 0) // �ý��� �߽���
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
// �� ������ ����: ���� �� ���� ���� �÷��� �ʱ�ȭ
bool Handle_S_LEAVE_ROOM_ACK(PacketSessionRef& session, Protocol::S_LEAVE_ROOM_ACK& pkt)
{
	if (pkt.success())
	{
		g_isInRoom = false; // �� ������ ���� �� �÷��� ����
		std::cout << "\n�濡�� ���������� �������ϴ�." << std::endl;
		std::cout << "���� �޴��� ���ư��ϴ�." << std::endl;
	}
	else
	{
		std::cout << "\n�� ������ ����! " << std::endl;
	}
	return true;
}


// ===================== ��Ÿ =====================
// �ʿ� �� Ȯ�� ����Ʈ(������/����Ʈ����/������ Ŀ�ǵ� ���� ��)
bool Handle_S_PRESENCE_NTF(PacketSessionRef& session, Protocol::S_PRESENCE_NTF& pkt) { return false; }
bool Handle_S_RATE_LIMIT_NTF(PacketSessionRef& session, Protocol::S_RATE_LIMIT_NTF& pkt) { return false; }
bool Handle_S_ADMIN_COMMAND_RES(PacketSessionRef& session, Protocol::S_ADMIN_COMMAND_RES& pkt) { return false; }
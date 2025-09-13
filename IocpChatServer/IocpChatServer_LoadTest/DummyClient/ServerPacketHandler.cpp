#include "pch.h"
#include "ServerPacketHandler.h"
#include "ServerSession.h" // ���� ���� ��� ����

// [����] Ŭ�� ���� ��Ŷ ó����
// - GPacketHandler: ��Ŷ ID �� ó�� �Լ� ���̺� (ServerPacketHandler::Init���� ä��)
// - �Ʒ� �ڵ鷯���� protobuf�� �Ľ̵� ���(��RES/��NTF)�� �޾Ƽ�
//   Ŭ�� ����(ServerSession) ���� + ������ �α� ��� ����

PacketHandlerFunc GPacketHandler[UINT16_MAX];

// ===================== INVALID =====================
// ������ ���� ��Ŷ ID�� ��Ī���� ���� �� (���̺� �ʱ�ȭ ����, �߸��� ������ ��)
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
    // TODO: log
    return false;
}

// ===================== LOGIN =====================
// �α��� ����: ���� üũ �� Ŭ�� �� ���� ���� ����
bool Handle_S_LOGIN_RES(PacketSessionRef& session, Protocol::S_LOGIN_RES& pkt)
{
    auto cli = std::dynamic_pointer_cast<ServerSession>(session);
    if (!cli) return false;

    if (pkt.status() == Protocol::CONNECT_OK)
    {
        cli->isLoggedIn = true; // ���� �� �α��� ���� on
        std::cout << "[" << cli->name << "] Login Success! Player ID: "
            << pkt.playerid() << " / Server Message: " << pkt.reason() << std::endl;
    }
    else
    {
        // ���� ������ ������ ������ reason�� �������
        std::cout << "[" << cli->name << "] Login Fail! Reason: " << pkt.reason() << std::endl;
    }
    return true;
}

// ===================== CREATE ROOM =====================
// �� ���� ����: ���� �� ���� ����(���� ����/roomId) ���� + ��� ��� ���
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

        // ������ ������ ���� ��� ������ ��� (�����/�׽�Ʈ�� ����)
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
// �� ���� ����: ���� �� ���� �÷���/roomId ������Ʈ
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

// ������ �����ߴٴ� �˸�(���� �ƴ� �ٸ� ������ ���� �̺�Ʈ)
bool Handle_S_JOIN_ROOM_NTF(PacketSessionRef& session, Protocol::S_JOIN_ROOM_NTF& pkt)
{
    std::cout << pkt.name() << " has entered!" << std::endl;
    return true;
}

// ===================== LEAVE ROOM =====================
// �� ������ ����: ���� �� ���� ���� �ʱ�ȭ
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
// �� ä�� �˸�: ����� ��� �ּ� ó�� (���ϸ� UI/�α׷� ����)
bool Handle_S_ROOM_CHAT_NTF(PacketSessionRef& session, Protocol::S_ROOM_CHAT_NTF& pkt)
{
    const auto& chat = pkt.chat();
    /*std::cout << "[Room " << pkt.roomid() << "] "
        << "<" << chat.senderid() << "> " << chat.message()
        << " (msgId=" << chat.messageid()
        << ", ts=" << chat.timestamp() << ")" << std::endl;*/
    return true;
}

// ===================== ��Ÿ =====================
// �ʿ� �� Ȯ�� ����Ʈ(������/����Ʈ����/������ Ŀ�ǵ� ���� ��)
bool Handle_S_PRESENCE_NTF(PacketSessionRef& session, Protocol::S_PRESENCE_NTF& pkt) { return false; }
bool Handle_S_RATE_LIMIT_NTF(PacketSessionRef& session, Protocol::S_RATE_LIMIT_NTF& pkt) { return false; }
bool Handle_S_ADMIN_COMMAND_RES(PacketSessionRef& session, Protocol::S_ADMIN_COMMAND_RES& pkt) { return false; }

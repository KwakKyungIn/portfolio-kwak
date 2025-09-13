#include "pch.h"
#include "ChatSession.h"
#include "ChatSessionManager.h"
#include "ClientPacketHandler.h"

// ������ ����Ǿ��� �� ȣ��Ǵ� �ݹ�
void ChatSession::OnConnected()
{
    // ���� ����� ChatSession�� ���� �Ŵ����� ���
    GSessionManager.Add(static_pointer_cast<ChatSession>(shared_from_this()));
}

// ������ �������� �� ȣ��Ǵ� �ݹ�
void ChatSession::OnDisconnected()
{
    // ������ ����� ChatSession�� ���� �Ŵ������� ����
    GSessionManager.Remove(static_pointer_cast<ChatSession>(shared_from_this()));
}

// ��Ŷ ���� �� ȣ��Ǵ� �ݹ�
void ChatSession::OnRecvPacket(BYTE* buffer, int32 len)
{
    PacketSessionRef session = GetPacketSessionRef();          // ���� ���� ���� ���
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer); // ���� �պκ��� ��Ŷ ����� ĳ����

    // TODO: ��Ŷ ID ��ȿ ���� �˻� �ʿ� (����/������)
    ClientPacketHandler::HandlePacket(session, buffer, len);   // ��Ŷ ó����� ����
}

// ��Ŷ ���� �Ϸ� �� ȣ��Ǵ� �ݹ�
void ChatSession::OnSend(int32 len)
{
    // ����� Ư���� ó�� ���� (�α׳� �������� Ȯ�� ����)
}

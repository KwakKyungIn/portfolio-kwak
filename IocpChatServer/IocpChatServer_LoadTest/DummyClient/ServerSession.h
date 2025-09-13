#pragma once
#include "pch.h"                 // [1] �׻� �ֻ��
#include "Session.h"
#include "ServerPacketHandler.h"
#include "Protocol.pb.h"
#include <string>
#include <atomic>

// [Ŭ�� �� ��]�� �ش��ϴ� ���� ���� + �ݹ� ó��
// - name: �α��� �� ������ ���� �г���
// - isLoggedIn/isInRoom/roomId: ���� ����/���� ���¸� ���ÿ��� ����
class ServerSession : public PacketSession
{
public:
    explicit ServerSession(const std::string& name_ = "") : name(name_) {}

    // per-client state (�׽�Ʈ/������ ���°���)
    std::string name;
    std::atomic<bool> isLoggedIn{ false };
    std::atomic<bool> isInRoom{ false };
    std::atomic<int32> roomId{ -1 };

    // OnConnected: ���� ���� ���� 1ȸ ȣ��
    // - ù �޽����� �α��� ��û�� ���� (�ڵ����ũ ����)
    void OnConnected() override
    {
        Protocol::C_LOGIN_REQ pkt;
        pkt.set_name(name);
        Send(ServerPacketHandler::MakeSendBuffer(pkt)); // ����ó�� ���/����ȭ ó��
    }

    // OnRecvPacket: ���� ���ۿ��� �ϼ��� ��Ŷ ������ ȣ���
    // - ServerPacketHandler::HandlePacket�� ��� id�� ���� ������ �ڵ鷯 ����
    void OnRecvPacket(BYTE* buffer, int32 len) override
    {
        auto self = GetPacketSessionRef();
        ServerPacketHandler::HandlePacket(self, buffer, len);
    }

    // ���� ���� �� ���� �ʱ�ȭ (������ �ø� ���)
    void OnDisconnected() override
    {
        isLoggedIn = false;
        isInRoom = false;
        roomId = -1;
    }
};

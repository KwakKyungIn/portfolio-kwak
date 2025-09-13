#pragma once
#include "Session.h"
#include "Player.h"

class ChatSession : public PacketSession
{
public:
    ~ChatSession()
    {
        // ������: ���� �Ҹ� �� �α� ���
        cout << "~ChatSession" << endl;
    }

    // ���� ���� ��ȭ ���� �ݹ��
    virtual void OnConnected() override;                  // ���� ���� ��
    virtual void OnDisconnected() override;               // ���� ���� ��
    virtual void OnRecvPacket(BYTE* buffer, int32 len) override; // ��Ŷ ���� ��
    virtual void OnSend(int32 len) override;              // ���� �Ϸ� ��

public:
    // ���ǿ��� �÷��̾� �ϳ��� �ٴ´�
    PlayerRef GetPlayer() { return _player; }
    void SetPlayer(PlayerRef player) { _player = player; }

private:
    PlayerRef _player; // �� ���ǿ� ����� �÷��̾� ����
};

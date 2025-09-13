#pragma once

class ChatSession;
using ChatSessionRef = shared_ptr<ChatSession>; // ChatSession ����Ʈ ������ ����

class ChatSessionManager
{
public:
    void Add(ChatSessionRef session);             // ���� ���
    void Remove(ChatSessionRef session);          // ���� ����
    void Broadcast(SendBufferRef sendBuffer);     // ��ü ���ǿ� �޽��� ��ε�ĳ��Ʈ

private:
    USE_LOCK;                       // ������ ���ü� ����
    std::set<ChatSessionRef> _sessions; // ���� ����� ���� ���
};

// �������� ������ �� �ְ� ����
extern ChatSessionManager GSessionManager;

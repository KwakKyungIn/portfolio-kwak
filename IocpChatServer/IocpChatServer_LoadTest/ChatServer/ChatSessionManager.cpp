#include "pch.h"
#include "ChatSessionManager.h"
#include "ChatSession.h"

// ���� ���� �Ŵ��� ��ü ����
ChatSessionManager GSessionManager;

// �� ������ ���
void ChatSessionManager::Add(ChatSessionRef session)
{
    WRITE_LOCK;                  // ���� �� ��Ƽ� ���ü� ����
    _sessions.insert(session);   // set�� ���� �߰�
}

// ������ ����
void ChatSessionManager::Remove(ChatSessionRef session)
{
    WRITE_LOCK;                  // ���� �� ��� �����ϰ� ����
    _sessions.erase(session);    // set���� ���� ����
}

// ��� ���ǿ� ��ε�ĳ��Ʈ �޽��� ����
void ChatSessionManager::Broadcast(SendBufferRef sendBuffer)
{
    WRITE_LOCK;                  // ���� ���� ����
    for (ChatSessionRef session : _sessions)
    {
        session->Send(sendBuffer); // �� ���ǿ� �޽��� ����
    }
}

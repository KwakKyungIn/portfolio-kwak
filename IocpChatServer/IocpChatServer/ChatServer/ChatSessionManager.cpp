#include "pch.h"
#include "ChatSessionManager.h"
#include "ChatSession.h"

// 전역 세션 매니저 객체 선언
ChatSessionManager GSessionManager;

// 새 세션을 등록
void ChatSessionManager::Add(ChatSessionRef session)
{
    WRITE_LOCK;                  // 쓰기 락 잡아서 동시성 보장
    _sessions.insert(session);   // set에 세션 추가
}

// 세션을 제거
void ChatSessionManager::Remove(ChatSessionRef session)
{
    WRITE_LOCK;                  // 쓰기 락 잡고 안전하게 제거
    _sessions.erase(session);    // set에서 세션 삭제
}

// 모든 세션에 브로드캐스트 메시지 전송
void ChatSessionManager::Broadcast(SendBufferRef sendBuffer)
{
    WRITE_LOCK;                  // 동시 접근 막기
    for (ChatSessionRef session : _sessions)
    {
        session->Send(sendBuffer); // 각 세션에 메시지 전송
    }
}

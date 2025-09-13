#pragma once

class ChatSession;
using ChatSessionRef = shared_ptr<ChatSession>; // ChatSession 스마트 포인터 정의

class ChatSessionManager
{
public:
    void Add(ChatSessionRef session);             // 세션 등록
    void Remove(ChatSessionRef session);          // 세션 제거
    void Broadcast(SendBufferRef sendBuffer);     // 전체 세션에 메시지 브로드캐스트

private:
    USE_LOCK;                       // 락으로 동시성 제어
    std::set<ChatSessionRef> _sessions; // 현재 연결된 세션 목록
};

// 전역으로 접근할 수 있게 선언
extern ChatSessionManager GSessionManager;

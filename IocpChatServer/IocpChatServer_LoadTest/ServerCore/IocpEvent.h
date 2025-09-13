#pragma once

class Session;

// IOCP에서 구분할 이벤트 타입
// - Connect/Disconnect/Accept: 세션 수립/해제 관련
// - Recv/Send: I/O 완료 통지
enum class EventType : uint8
{
    Connect,
    Disconnect,
    Accept,
    //PreRecv,          // 필요 시 활성화
    Recv,
    Send
};

/*--------------
    IocpEvent
    - OVERLAPPED를 상속하여 Win32 비동기 I/O에 직접 사용
    - owner: 이 이벤트를 처리할 IocpObject (세션/리스너 등)
---------------*/
class IocpEvent : public OVERLAPPED
{
public:
    explicit IocpEvent(EventType type);

    // 재사용 전 OVERLAPPED 필드 초기화
    void Init();

public:
    EventType     eventType; // 이벤트 종류(디스패치 분기용)
    IocpObjectRef owner;     // 이벤트 소유 개체(콜백 대상)
};

/*----------------
    ConnectEvent
    - 비동기 연결 완료(클라→서버) 시 사용
-----------------*/
class ConnectEvent : public IocpEvent
{
public:
    ConnectEvent() : IocpEvent(EventType::Connect) {}
};

/*--------------------
    DisconnectEvent
    - 연결 종료 처리(정상/비정상) 시 사용
----------------------*/
class DisconnectEvent : public IocpEvent
{
public:
    DisconnectEvent() : IocpEvent(EventType::Disconnect) {}
};

/*----------------
    AcceptEvent
    - 서버 리스너의 AcceptEx 완료 시 사용
    - session: 받아들인 세션 핸들 보관
-----------------*/
class AcceptEvent : public IocpEvent
{
public:
    AcceptEvent() : IocpEvent(EventType::Accept) {}

public:
    SessionRef session = nullptr; // 새로 생성된 세션
};

/*----------------
    RecvEvent
    - WSARecv/Recv 완료 통지
-----------------*/
class RecvEvent : public IocpEvent
{
public:
    RecvEvent() : IocpEvent(EventType::Recv) {}
};

/*----------------
    SendEvent
    - WSASend/Send 완료 통지
    - sendBuffers: 전송한 SendBuffer 레퍼런스들(수명 관리)
    - wsaBufs: SG-WSASend용 버퍼 배열(재사용/추적)
-----------------*/
class SendEvent : public IocpEvent
{
public:
    SendEvent() : IocpEvent(EventType::Send) {}

    Vector<SendBufferRef> sendBuffers; // 전송 데이터 소유(레퍼런스 유지)
    Vector<WSABUF>        wsaBufs;     // Scatter–Gather 버퍼들
};

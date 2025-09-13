#pragma once
#include "IocpCore.h"
#include "NetAddress.h"

class AcceptEvent;
class ServerService;

/*--------------
    Listener
    - 리슨 소켓을 보유하고 IOCP에 등록
    - AcceptEx를 N개 선등록하여 동시 수용(accept pipeline)
    - 완료 시 새 Session 초기화 후 다시 Accept 재무장
---------------*/
class Listener : public IocpObject
{
public:
    Listener() = default;
    ~Listener();

public:
    // 외부 API: 리슨 시작
    // - service: 세션 생성/환경 접근(스레드, 주소, 코어 등)
    // - 내부에서 CreateSocket/Bind/Listen/IOCP 등록/AcceptEx N개 예약
    bool StartAccept(ServerServiceRef service);

    // 외부 API: 리슨 소켓만 닫기
    void CloseSocket();

public:
    // IocpObject 인터페이스
    virtual HANDLE GetHandle() override;                                   // IOCP에 등록될 HANDLE
    virtual void   Dispatch(class IocpEvent* iocpEvent, int32 numOfBytes = 0) override; // Accept 완료 콜백

private:
    // 내부: AcceptEx 등록 (비동기 착수)
    void RegisterAccept(AcceptEvent* acceptEvent);

    // 내부: Accept 완료 처리 (옵션 상속/peer 주소/세션 활성화)
    void ProcessAccept(AcceptEvent* acceptEvent);

protected:
    // 리슨용 소켓 핸들
    SOCKET _socket = INVALID_SOCKET;

    // 사전 예약된 Accept 이벤트 풀 (재사용)
    Vector<AcceptEvent*> _acceptEvents;

    // 세션 생성/환경 접근을 위한 서비스 핸들
    ServerServiceRef _service;
};

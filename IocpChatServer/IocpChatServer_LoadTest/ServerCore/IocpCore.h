#pragma once

/*----------------
    IocpObject
    - IOCP에 등록되는 모든 개체의 공통 인터페이스
    - 개체별 HANDLE 제공 + 완료 통지 시 동작 정의
-----------------*/
class IocpObject : public enable_shared_from_this<IocpObject>
{
public:
    // IOCP에 등록할 Win32 HANDLE
    virtual HANDLE GetHandle() abstract;

    // IOCP 완료 통지 시 호출되는 콜백
    // - iocpEvent: 어떤 종류의 이벤트인지 (Recv/Send/Accept 등)
    // - numOfBytes: 완료된 바이트 수(Recv/Send에서 사용)
    virtual void Dispatch(class IocpEvent* iocpEvent, int32 numOfBytes = 0) abstract;
};

/*--------------
    IocpCore
    - IOCP 포트를 생성하고, IocpObject를 등록/디스패치
    - 스레드 루프에서 Dispatch(timeout) 호출로 이벤트 처리
---------------*/
class IocpCore
{
public:
    IocpCore();
    ~IocpCore();

    // 포트 핸들 (필요 시 외부에서 조회)
    HANDLE GetHandle() { return _iocpHandle; }

    // HANDLE을 IOCP 포트에 연결
    bool Register(IocpObjectRef iocpObject);

    // 완료 큐에서 이벤트 하나 꺼내서 분배
    // - true: 뭔가 처리함
    // - false: 타임아웃 등으로 처리할 이벤트가 없음
    bool Dispatch(uint32 timeoutMs = INFINITE);

private:
    HANDLE _iocpHandle; // IOCP 포트 핸들
};

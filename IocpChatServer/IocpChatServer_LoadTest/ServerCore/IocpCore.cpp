#include "pch.h"
#include "IocpCore.h"
#include "IocpEvent.h"

/*--------------
    IocpCore
    - IOCP 포트 생성/관리
    - 개체 등록(Register) 및 완료 통지(Dispatch) 처리
---------------*/

IocpCore::IocpCore()
{
    // IOCP 포트 생성 (스레드 수=0 → 시스템이 적절히 스레드 스케줄링)
    _iocpHandle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
    ASSERT_CRASH(_iocpHandle != INVALID_HANDLE_VALUE);
}

IocpCore::~IocpCore()
{
    // 포트 핸들 정리
    ::CloseHandle(_iocpHandle);
}

bool IocpCore::Register(IocpObjectRef iocpObject)
{
    // 개체의 HANDLE을 IOCP 포트에 연결
    // key는 0 사용(별도 키 관리가 필요 없을 때)
    return ::CreateIoCompletionPort(iocpObject->GetHandle(), _iocpHandle, /*key*/0, 0);
}

bool IocpCore::Dispatch(uint32 timeoutMs)
{
    // 완료 통지 결과를 담을 변수들
    DWORD      numOfBytes = 0;      // 완료된 바이트 수
    ULONG_PTR  key = 0;             // CreateIoCompletionPort에서 준 key
    IocpEvent* iocpEvent = nullptr; // OVERLAPPED → IocpEvent*

    // 완료 큐에서 하나 꺼냄 (timeoutMs 동안 대기)
    if (::GetQueuedCompletionStatus(_iocpHandle, OUT & numOfBytes, OUT & key, OUT reinterpret_cast<LPOVERLAPPED*>(&iocpEvent), timeoutMs))
    {
        // 정상 완료: 이벤트의 owner(IocpObject)에게 분배
        IocpObjectRef iocpObject = iocpEvent->owner;
        iocpObject->Dispatch(iocpEvent, numOfBytes);
    }
    else
    {
        // 실패(또는 일부 케이스) 처리
        int32 errCode = ::WSAGetLastError();
        switch (errCode)
        {
        case WAIT_TIMEOUT:
            // 타임아웃: 처리할 이벤트 없음
            return false;
        default:
            // 에러이지만 OVERLAPPED는 유효할 수 있음 → 동일하게 Dispatch
            // (상대가 끊겼다든지 등의 에러는 각 개체에서 처리)
            // TODO: 필요 시 에러 로깅
            IocpObjectRef iocpObject = iocpEvent->owner;
            iocpObject->Dispatch(iocpEvent, numOfBytes);
            break;
        }
    }

    return true; // 이벤트 하나 처리함
}

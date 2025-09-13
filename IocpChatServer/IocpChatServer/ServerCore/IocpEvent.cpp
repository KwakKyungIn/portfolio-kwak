#include "pch.h"
#include "IocpEvent.h"

/*--------------
    IocpEvent
    - OVERLAPPED 기반 이벤트 객체
    - Init()으로 재사용 전 필드 초기화
---------------*/

IocpEvent::IocpEvent(EventType type) : eventType(type)
{
    Init();
}

void IocpEvent::Init()
{
    // OVERLAPPED 기본 필드 초기화 (재사용 대비)
    OVERLAPPED::hEvent = 0;
    OVERLAPPED::Internal = 0;
    OVERLAPPED::InternalHigh = 0;
    OVERLAPPED::Offset = 0;
    OVERLAPPED::OffsetHigh = 0;
}

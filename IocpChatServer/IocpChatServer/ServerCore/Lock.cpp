#include "pch.h"
#include "Lock.h"
#include "CoreTLS.h"
#include "DeadLockProfiler.h"

void Lock::WriteLock(const char* name)
{
#if _DEBUG
    // 디버그 모드일 때는 데드락 탐지기에도 기록
    GDeadLockProfiler->PushLock(name);
#endif

    // 이미 내가 소유한 쓰기 락인지 확인 (재진입 허용)
    const uint32 lockThreadId = (_lockFlag.load() & WRITE_THREAD_MASK) >> 16;
    if (LThreadId == lockThreadId)
    {
        _writeCount++; // 중첩 카운트 증가
        return;        // 내 소유이므로 바로 성공
    }

    const int64 beginTick = ::GetTickCount64();

    // 소유자가 없을 때 CAS로 소유권 획득 시도
    const uint32 desired = ((LThreadId << 16) & WRITE_THREAD_MASK);
    while (true)
    {
        for (uint32 spinCount = 0; spinCount < MAX_SPIN_COUNT; ++spinCount)
        {
            uint32 expected = EMPTY_FLAG;
            if (_lockFlag.compare_exchange_strong(OUT expected, desired))
            {
                _writeCount++;
                return; // 성공적으로 쓰기 락 획득
            }
        }

        // 일정 시간 넘게 못 얻으면 Crash (디버깅용)
        if (::GetTickCount64() - beginTick > ACQUIRE_TIMEOUT_TICK)
        {
            CRASH("LOCK_TIMEOUT");
        }

        this_thread::yield(); // 다른 스레드에게 CPU 양보
    }
}

void Lock::WriteUnlock(const char* name)
{
#if _DEBUG
    // 데드락 프로파일러에서 제거
    GDeadLockProfiler->PopLock(name);
#endif

    // ReadLock 이 걸려있는 상태에서 WriteUnlock 호출은 잘못된 순서
    if ((_lockFlag.load() & READ_COUNT_MASK) != 0)
    {
        CRASH("INVALID_UNLOCK_ORDER");
    }

    // 중첩된 쓰기 락 해제
    const int32 lockCount = --_writeCount;
    if (lockCount == 0)
        _lockFlag.store(EMPTY_FLAG); // 실제 락 반환
}

void Lock::ReadLock(const char* name)
{
#if _DEBUG
    GDeadLockProfiler->PushLock(name);
#endif

    // 내가 이미 쓰기 락을 가지고 있다면 읽기 카운트만 올림
    const uint32 lockThreadId = (_lockFlag.load() & WRITE_THREAD_MASK) >> 16;
    if (LThreadId == lockThreadId)
    {
        _lockFlag.fetch_add(1);
        return; // 내 쓰기 락 아래에서는 읽기 가능
    }

    const int64 beginTick = ::GetTickCount64();
    while (true)
    {
        for (uint32 spinCount = 0; spinCount < MAX_SPIN_COUNT; ++spinCount)
        {
            // 현재 읽기 카운트를 읽고 +1 시도
            uint32 expected = (_lockFlag.load() & READ_COUNT_MASK);
            if (_lockFlag.compare_exchange_strong(expected, expected + 1))
                return; // 성공적으로 ReadLock 획득
        }

        if (::GetTickCount64() - beginTick > ACQUIRE_TIMEOUT_TICK)
        {
            CRASH("LOCK_TIMEOUT");
        }

        this_thread::yield();
    }
}

void Lock::ReadUnlock(const char* name)
{
#if _DEBUG
    GDeadLockProfiler->PopLock(name);
#endif

    // 읽기 카운트를 줄이는데 결과가 0이면 unlock 순서가 꼬인 것
    if ((_lockFlag.fetch_sub(1) && READ_COUNT_MASK) == 0)
        CRASH("MULTIPLE_UNLOCK");
}

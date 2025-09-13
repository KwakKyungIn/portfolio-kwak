#pragma once
#include "Types.h"

/*----------------
    RW SpinLock
-----------------*/
/*
    - 읽기/쓰기 겸용 스핀락 구현
    - 여러 스레드가 동시에 ReadLock 은 가능하지만
      WriteLock 은 오직 하나의 스레드만 가능
    - 내부적으로 32비트 플래그를 사용해서
      상위 16비트는 쓰기 잠금 소유자(ThreadId),
      하위 16비트는 읽기 잠금 카운트로 관리
*/
class Lock
{
    enum : uint32
    {
        ACQUIRE_TIMEOUT_TICK = 10000,   // 일정 시간 이상 잠금 못 얻으면 Crash
        MAX_SPIN_COUNT = 5000,          // CAS 시도 횟수(스핀락)
        WRITE_THREAD_MASK = 0xFFFF'0000,// 쓰기 잠금 소유자 ID 저장 영역
        READ_COUNT_MASK = 0x0000'FFFF,  // 현재 읽기 잠금 개수
        EMPTY_FLAG = 0x0000'0000        // 아무도 안 쓰는 상태
    };

public:
    void WriteLock(const char* name);   // 쓰기 잠금 시도
    void WriteUnlock(const char* name); // 쓰기 잠금 해제
    void ReadLock(const char* name);    // 읽기 잠금 시도
    void ReadUnlock(const char* name);  // 읽기 잠금 해제

private:
    Atomic<uint32> _lockFlag = EMPTY_FLAG; // 락 상태 저장 (비트마스크 방식)
    uint16 _writeCount = 0;                // 쓰기 잠금 중첩 횟수
};

/*----------------
    LockGuards
-----------------*/
/*
    - RAII 방식으로 Lock 을 관리
    - 생성자에서 잠금, 소멸자에서 해제
    - 코드 블록을 벗어날 때 자동으로 해제되므로 안전
*/
class ReadLockGuard
{
public:
    ReadLockGuard(Lock& lock, const char* name)
        : _lock(lock), _name(name) {
        _lock.ReadLock(name);
    }
    ~ReadLockGuard() { _lock.ReadUnlock(_name); }

private:
    Lock& _lock;
    const char* _name;
};

class WriteLockGuard
{
public:
    WriteLockGuard(Lock& lock, const char* name)
        : _lock(lock), _name(name) {
        _lock.WriteLock(name);
    }
    ~WriteLockGuard() { _lock.WriteUnlock(_name); }

private:
    Lock& _lock;
    const char* _name;
};

#pragma once

#define OUT

/*---------------
      Lock
---------------*/
// 여러 개의 락 배열 사용
#define USE_MANY_LOCKS(count)    Lock _locks[count];
// 락 1개만 사용
#define USE_LOCK                 USE_MANY_LOCKS(1)
// 읽기 락 매크로
#define READ_LOCK_IDX(idx)       ReadLockGuard readLockGuard_##idx(_locks[idx], typeid(this).name());
#define READ_LOCK                READ_LOCK_IDX(0)
// 쓰기 락 매크로
#define WRITE_LOCK_IDX(idx)      WriteLockGuard writeLockGuard_##idx(_locks[idx], typeid(this).name());
#define WRITE_LOCK               WRITE_LOCK_IDX(0)

/*---------------
      Crash
---------------*/
// 강제로 크래시 발생시켜 디버깅 시점 확인
#define CRASH(cause)                        \
{                                           \
    uint32* crash = nullptr;                \
    __analysis_assume(crash != nullptr);    \
    *crash = 0xDEADBEEF;                    \
}

// 조건 실패 시 강제 크래시
#define ASSERT_CRASH(expr)                  \
{                                           \
    if (!(expr))                            \
    {                                       \
        CRASH("ASSERT_CRASH");              \
        __analysis_assume(expr);            \
    }                                       \
}

#pragma once

#define OUT

/*---------------
      Lock
---------------*/
// ���� ���� �� �迭 ���
#define USE_MANY_LOCKS(count)    Lock _locks[count];
// �� 1���� ���
#define USE_LOCK                 USE_MANY_LOCKS(1)
// �б� �� ��ũ��
#define READ_LOCK_IDX(idx)       ReadLockGuard readLockGuard_##idx(_locks[idx], typeid(this).name());
#define READ_LOCK                READ_LOCK_IDX(0)
// ���� �� ��ũ��
#define WRITE_LOCK_IDX(idx)      WriteLockGuard writeLockGuard_##idx(_locks[idx], typeid(this).name());
#define WRITE_LOCK               WRITE_LOCK_IDX(0)

/*---------------
      Crash
---------------*/
// ������ ũ���� �߻����� ����� ���� Ȯ��
#define CRASH(cause)                        \
{                                           \
    uint32* crash = nullptr;                \
    __analysis_assume(crash != nullptr);    \
    *crash = 0xDEADBEEF;                    \
}

// ���� ���� �� ���� ũ����
#define ASSERT_CRASH(expr)                  \
{                                           \
    if (!(expr))                            \
    {                                       \
        CRASH("ASSERT_CRASH");              \
        __analysis_assume(expr);            \
    }                                       \
}

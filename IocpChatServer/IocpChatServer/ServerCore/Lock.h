#pragma once
#include "Types.h"

/*----------------
    RW SpinLock
-----------------*/
/*
    - �б�/���� ��� ���ɶ� ����
    - ���� �����尡 ���ÿ� ReadLock �� ����������
      WriteLock �� ���� �ϳ��� �����常 ����
    - ���������� 32��Ʈ �÷��׸� ����ؼ�
      ���� 16��Ʈ�� ���� ��� ������(ThreadId),
      ���� 16��Ʈ�� �б� ��� ī��Ʈ�� ����
*/
class Lock
{
    enum : uint32
    {
        ACQUIRE_TIMEOUT_TICK = 10000,   // ���� �ð� �̻� ��� �� ������ Crash
        MAX_SPIN_COUNT = 5000,          // CAS �õ� Ƚ��(���ɶ�)
        WRITE_THREAD_MASK = 0xFFFF'0000,// ���� ��� ������ ID ���� ����
        READ_COUNT_MASK = 0x0000'FFFF,  // ���� �б� ��� ����
        EMPTY_FLAG = 0x0000'0000        // �ƹ��� �� ���� ����
    };

public:
    void WriteLock(const char* name);   // ���� ��� �õ�
    void WriteUnlock(const char* name); // ���� ��� ����
    void ReadLock(const char* name);    // �б� ��� �õ�
    void ReadUnlock(const char* name);  // �б� ��� ����

private:
    Atomic<uint32> _lockFlag = EMPTY_FLAG; // �� ���� ���� (��Ʈ����ũ ���)
    uint16 _writeCount = 0;                // ���� ��� ��ø Ƚ��
};

/*----------------
    LockGuards
-----------------*/
/*
    - RAII ������� Lock �� ����
    - �����ڿ��� ���, �Ҹ��ڿ��� ����
    - �ڵ� ����� ��� �� �ڵ����� �����ǹǷ� ����
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

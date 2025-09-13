#include "pch.h"
#include "ThreadManager.h"
#include "CoreTLS.h"
#include "CoreGlobal.h"

/*--------------------------------
        ThreadManager
    ������ ����/���� Ŭ����
    - TLS �ʱ�ȭ
    - ������ ����/���� ����
---------------------------------*/

ThreadManager::ThreadManager()
{
    // ���� �����嵵 TLS �ʱ�ȭ
    InitTLS();
}

ThreadManager::~ThreadManager()
{
    // ��� �����尡 ���� ������ ���
    Join();
}

void ThreadManager::Launch(function<void()> callback)
{
    // ���ο� ������ ���� �� �� �ɰ� ����Ʈ�� �߰�
    LockGuard guard(_lock);
    _threads.push_back(thread([=]()
        {
            // �� �����忡�� TLS �ʱ�ȭ
            InitTLS();

            // ������ �ݹ� �Լ�
            callback();

            // TLS ���� (����� ��� ����)
            DestroyTLS();
        }));
}

void ThreadManager::Join()
{
    // ������ ��� �����带 join
    for (thread& t : _threads)
    {
        if (t.joinable())
        {
            t.join(); // ������ ���� ���
        }
    }
    _threads.clear(); // ���� �ʱ�ȭ
}

void ThreadManager::InitTLS()
{
    // ������ ������ ID�� �ο�
    static Atomic<uint32> SThreadId = 1;
    LThreadId = SThreadId.fetch_add(1);
}

void ThreadManager::DestroyTLS()
{
    // ����� ������ ���ҽ� ���� (Ȯ�� ����)
}

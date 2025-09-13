#pragma once
#include <thread>
#include <functional>

/*--------------------------------
        ThreadManager
    - ���� ���� �����带 ����� ����
    - TLS �ʱ�ȭ/���� ����
---------------------------------*/

class ThreadManager
{
public:
    ThreadManager();
    ~ThreadManager();

    // ���ο� �����带 �����ϰ� callback �Լ� ����
    void Launch(function<void()> callback);

    // ��� ������ ���� ���
    void Join();

    // TLS �ʱ�ȭ / ����
    static void InitTLS();
    static void DestroyTLS();

private:
    Mutex            _lock;       // ������ ����Ʈ ��ȣ�� ��
    vector<thread>   _threads;    // ������ ������ ���
};

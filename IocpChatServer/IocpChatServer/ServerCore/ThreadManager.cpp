#include "pch.h"
#include "ThreadManager.h"
#include "CoreTLS.h"
#include "CoreGlobal.h"

/*--------------------------------
        ThreadManager
    쓰레드 생성/관리 클래스
    - TLS 초기화
    - 쓰레드 시작/종료 관리
---------------------------------*/

ThreadManager::ThreadManager()
{
    // 메인 쓰레드도 TLS 초기화
    InitTLS();
}

ThreadManager::~ThreadManager()
{
    // 모든 쓰레드가 끝날 때까지 대기
    Join();
}

void ThreadManager::Launch(function<void()> callback)
{
    // 새로운 쓰레드 생성 시 락 걸고 리스트에 추가
    LockGuard guard(_lock);
    _threads.push_back(thread([=]()
        {
            // 새 쓰레드에서 TLS 초기화
            InitTLS();

            // 실행할 콜백 함수
            callback();

            // TLS 정리 (현재는 비어 있음)
            DestroyTLS();
        }));
}

void ThreadManager::Join()
{
    // 생성된 모든 쓰레드를 join
    for (thread& t : _threads)
    {
        if (t.joinable())
        {
            t.join(); // 쓰레드 종료 대기
        }
    }
    _threads.clear(); // 벡터 초기화
}

void ThreadManager::InitTLS()
{
    // 고유한 스레드 ID를 부여
    static Atomic<uint32> SThreadId = 1;
    LThreadId = SThreadId.fetch_add(1);
}

void ThreadManager::DestroyTLS()
{
    // 현재는 정리할 리소스 없음 (확장 가능)
}

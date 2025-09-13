#pragma once
#include <thread>
#include <functional>

/*--------------------------------
        ThreadManager
    - 여러 개의 쓰레드를 만들고 관리
    - TLS 초기화/정리 지원
---------------------------------*/

class ThreadManager
{
public:
    ThreadManager();
    ~ThreadManager();

    // 새로운 쓰레드를 실행하고 callback 함수 수행
    void Launch(function<void()> callback);

    // 모든 쓰레드 종료 대기
    void Join();

    // TLS 초기화 / 해제
    static void InitTLS();
    static void DestroyTLS();

private:
    Mutex            _lock;       // 쓰레드 리스트 보호용 락
    vector<thread>   _threads;    // 생성된 쓰레드 목록
};

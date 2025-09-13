#pragma once
#include "DBConnection.h"
#include <queue>
#include <condition_variable>

class DBConnectionPool
{
public:
    DBConnectionPool() = default;
    ~DBConnectionPool();

    // 지정한 개수만큼 DB 연결을 생성해서 풀에 넣음
    bool Connect(int32 connectionCount, const WCHAR* connectionString);

    // 모든 연결과 자원 해제
    void Clear();

    // 풀에서 DB 연결 하나 꺼내옴 (없으면 대기)
    DBConnection* Pop();

    // 사용이 끝난 연결을 풀에 반납
    void Push(DBConnection* connection);

private:
    // ODBC 환경 핸들
    SQLHENV _environment = SQL_NULL_HANDLE;

    // DB 연결들을 보관하는 큐
    std::queue<DBConnection*> _connections;

    // 동기화 도구 (멀티스레드 안전)
    std::mutex _mutex;
    std::condition_variable _cv;

    // 풀 종료 여부 플래그
    bool _shutdown = false;
};

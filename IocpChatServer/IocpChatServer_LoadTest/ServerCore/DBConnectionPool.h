#pragma once
#include "DBConnection.h"
#include <queue>
#include <condition_variable>

class DBConnectionPool
{
public:
    DBConnectionPool() = default;
    ~DBConnectionPool();

    // ������ ������ŭ DB ������ �����ؼ� Ǯ�� ����
    bool Connect(int32 connectionCount, const WCHAR* connectionString);

    // ��� ����� �ڿ� ����
    void Clear();

    // Ǯ���� DB ���� �ϳ� ������ (������ ���)
    DBConnection* Pop();

    // ����� ���� ������ Ǯ�� �ݳ�
    void Push(DBConnection* connection);

private:
    // ODBC ȯ�� �ڵ�
    SQLHENV _environment = SQL_NULL_HANDLE;

    // DB ������� �����ϴ� ť
    std::queue<DBConnection*> _connections;

    // ����ȭ ���� (��Ƽ������ ����)
    std::mutex _mutex;
    std::condition_variable _cv;

    // Ǯ ���� ���� �÷���
    bool _shutdown = false;
};

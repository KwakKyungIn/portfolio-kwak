#include "pch.h"
#include "DBConnectionPool.h"
#include <iostream>
#include <chrono>

DBConnectionPool::~DBConnectionPool()
{
    // ��ü ���� �� Ǯ ����
    Clear();
}

bool DBConnectionPool::Connect(int32 connectionCount, const WCHAR* connectionString)
{
    std::unique_lock<std::mutex> lock(_mutex);

    // ODBC ȯ�� �ڵ� ����
    if (::SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &_environment) != SQL_SUCCESS)
    {
        std::wcout << L"SQLAllocHandle(SQL_HANDLE_ENV) failed." << std::endl;
        return false;
    }

    // ODBC ���� 3.0���� ����
    if (::SQLSetEnvAttr(_environment, SQL_ATTR_ODBC_VERSION,
        reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0) != SQL_SUCCESS)
    {
        std::wcout << L"SQLSetEnvAttr failed." << std::endl;
        return false;
    }

    // ������ ������ŭ DB ���� ���� �� Ǯ�� ����
    for (int32 i = 0; i < connectionCount; i++)
    {
        DBConnection* connection = xnew<DBConnection>();
        if (connection->Connect(_environment, connectionString) == false)
        {
            std::wcout << L"DBConnection::Connect failed." << std::endl;
            xdelete(connection);
            return false;
        }

        _connections.push(connection);
    }

    return true;
}

void DBConnectionPool::Clear()
{
    std::unique_lock<std::mutex> lock(_mutex);
    _shutdown = true;

    // ��� ���� Pop ������ �����
    _cv.notify_all();

    // Ǯ�� ���� ���� ��� ����
    while (!_connections.empty())
    {
        DBConnection* conn = _connections.front();
        _connections.pop();
        xdelete(conn);
    }

    // ȯ�� �ڵ� ����
    if (_environment != SQL_NULL_HANDLE)
    {
        ::SQLFreeHandle(SQL_HANDLE_ENV, _environment);
        _environment = SQL_NULL_HANDLE;
    }

}

DBConnection* DBConnectionPool::Pop()
{
    // ��� �ð� ���� ����
    auto t0 = std::chrono::steady_clock::now();

    std::unique_lock<std::mutex> lock(_mutex);

    // ������ ����ų� ����� ������ ���
    _cv.wait(lock, [this] { return !_connections.empty() || _shutdown; });

    if (_shutdown)
        return nullptr;

    // ��� ������ ���� �ϳ� ������
    DBConnection* connection = _connections.front();
    _connections.pop();

    // ��� �ð� ��� �� ��Ʈ�� ���
    auto waitMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();

    return connection;
}

void DBConnectionPool::Push(DBConnection* connection)
{
    {
        std::unique_lock<std::mutex> lock(_mutex);

        // ����� ������ �ٽ� Ǯ�� �ݳ�
        _connections.push(connection);

    }

    // ��� ���� Pop ������ �����
    _cv.notify_one();
}

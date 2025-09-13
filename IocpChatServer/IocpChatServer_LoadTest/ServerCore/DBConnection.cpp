#include "pch.h"
#include "DBConnection.h"
#include <iostream>
#include <locale>
#include "Metrics.h" 


bool DBConnection::Connect(SQLHENV henv, const WCHAR* connectionString)
{
    // DB ���� �ڵ� �Ҵ�
    if (::SQLAllocHandle(SQL_HANDLE_DBC, henv, &_connection) != SQL_SUCCESS)
        return false;

    WCHAR stringBuffer[MAX_PATH] = { 0 };
    ::wcscpy_s(stringBuffer, connectionString);

    WCHAR resultString[MAX_PATH] = { 0 };
    SQLSMALLINT resultStringLen = 0;

    // ���� DB ���� �õ�
    SQLRETURN ret = ::SQLDriverConnectW(
        _connection,
        NULL,
        reinterpret_cast<SQLWCHAR*>(stringBuffer),
        _countof(stringBuffer),
        OUT reinterpret_cast<SQLWCHAR*>(resultString),
        _countof(resultString),
        OUT & resultStringLen,
        SQL_DRIVER_NOPROMPT
    );

    // statement �ڵ� �Ҵ�
    if (::SQLAllocHandle(SQL_HANDLE_STMT, _connection, &_statement) != SQL_SUCCESS)
        return false;

    return (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO);
}

void DBConnection::Clear()
{
    // �ڵ� ����
    if (_connection != SQL_NULL_HANDLE)
    {
        ::SQLFreeHandle(SQL_HANDLE_DBC, _connection);
        _connection = SQL_NULL_HANDLE;
    }

    if (_statement != SQL_NULL_HANDLE)
    {
        ::SQLFreeHandle(SQL_HANDLE_STMT, _statement);
        _statement = SQL_NULL_HANDLE;
    }
}

bool DBConnection::Prepare(const WCHAR* query)
{
    // ���� �غ� (����������)
    SQLRETURN ret = ::SQLPrepareW(_statement, (SQLWCHAR*)query, SQL_NTSL);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
        HandleError(ret);
        GMetrics.db_prepare_fail.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

bool DBConnection::Execute()
{
    // �غ�� ���� ����
    SQLRETURN ret = ::SQLExecute(_statement);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
        HandleError(ret);
        GMetrics.db_exec_fail.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    GMetrics.db_exec_ok.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool DBConnection::Execute(const WCHAR* query)
{
    // ���� �ٷ� ���� (Prepare ���ϰ� �ٷ�)
    SQLRETURN ret = ::SQLExecDirectW(_statement, (SQLWCHAR*)query, SQL_NTSL);
    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
    {
        GMetrics.db_exec_ok.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    HandleError(ret);
    GMetrics.db_exec_fail.fetch_add(1, std::memory_order_relaxed);
    return false;
}

bool DBConnection::Fetch()
{
    // ����¿��� ���� �� �ҷ�����
    SQLRETURN ret = ::SQLFetch(_statement);

    switch (ret)
    {
    case SQL_SUCCESS:
    case SQL_SUCCESS_WITH_INFO:
        GMetrics.db_fetch_ok.fetch_add(1, std::memory_order_relaxed);
        return true;
    case SQL_NO_DATA:
        GMetrics.db_fetch_no_data.fetch_add(1, std::memory_order_relaxed);
        return false;
    case SQL_ERROR:
        HandleError(ret);
        GMetrics.db_fetch_fail.fetch_add(1, std::memory_order_relaxed);
        return false;
    default:
        // ���� ���� ���� ���(SQL_STILL_EXECUTING ��)
        return true;
    }
}

int32 DBConnection::GetRowCount()
{
    // ����� ���� ��� �� �� ��������
    SQLLEN count = 0;
    SQLRETURN ret = ::SQLRowCount(_statement, OUT & count);

    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
        return static_cast<int32>(count);

    return -1;
}

void DBConnection::Unbind()
{
    // ���� ���� ���ε�/Ŀ�� ����
    ::SQLFreeStmt(_statement, SQL_CLOSE);         // Ŀ�� �ݱ�
    ::SQLFreeStmt(_statement, SQL_RESET_PARAMS);  // �Ķ���� ���ε� ����
    ::SQLFreeStmt(_statement, SQL_UNBIND);        // �÷� ���ε� ����

    GMetrics.db_unbind_calls.fetch_add(1, std::memory_order_relaxed);
}

bool DBConnection::BindParam(SQLUSMALLINT paramIndex, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN len, SQLPOINTER ptr, SQLLEN* index)
{
    // ���� �Ķ���� ���ε�
    SQLRETURN ret = ::SQLBindParameter(_statement, paramIndex, SQL_PARAM_INPUT, cType, sqlType, len, 0, ptr, 0, index);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
        HandleError(ret);
        return false;
    }

    return true;
}

bool DBConnection::BindCol(SQLUSMALLINT columnIndex, SQLSMALLINT cType, SQLULEN len, SQLPOINTER value, SQLLEN* index)
{
    // ��� �÷� ���ε�
    SQLRETURN ret = ::SQLBindCol(_statement, columnIndex, cType, value, len, index);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
        HandleError(ret);
        return false;
    }

    return true;
}

void DBConnection::HandleError(SQLRETURN ret)
{
    if (ret == SQL_SUCCESS)
        return;

    SQLSMALLINT index = 1;
    SQLWCHAR sqlState[MAX_PATH] = { 0 };
    SQLINTEGER nativeErr = 0;
    SQLWCHAR errMsg[MAX_PATH] = { 0 };
    SQLSMALLINT msgLen = 0;
    SQLRETURN errorRet = 0;

    while (true)
    {
        errorRet = ::SQLGetDiagRecW(
            SQL_HANDLE_STMT,
            _statement,
            index,
            sqlState,
            OUT & nativeErr,
            errMsg,
            _countof(errMsg),
            OUT & msgLen
        );

        if (errorRet == SQL_NO_DATA)
            break;

        if (errorRet != SQL_SUCCESS && errorRet != SQL_SUCCESS_WITH_INFO)
            break;

        // ���� �޽��� ��� (���� �α�)
        std::wcout.imbue(std::locale("kor"));
        std::wcout << L"DB Error: " << errMsg << std::endl;

        index++;
    }
}

#pragma once
#include <sql.h>
#include <sqlext.h>

class DBConnection
{
public:
	// DB ���� �õ� (ȯ�� �ڵ�� ���� ���ڿ� �ʿ�)
	bool Connect(SQLHENV henv, const WCHAR* connectionString);

	// ����/statement ����
	void Clear();

	// ���� �غ� (PreparedStatement ��)
	bool Prepare(const WCHAR* query);

	// �غ�� ���� ����
	bool Execute();

	// ���� ���� ���� (Prepare ���� �ٷ�)
	bool Execute(const WCHAR* query);

	// ����¿��� ���� �� �ҷ�����
	bool Fetch();

	// ����� ������ ��� �� ���� Ȯ��
	int32 GetRowCount();

	// statement ���� �ʱ�ȭ (Ŀ�� �ݱ�, ���ε� ����)
	void Unbind();

public:
	// ���� �Ķ���� ���ε� (INSERT, UPDATE � ���)
	// index�� ��� �Ķ���� ���̸� ���� �� ���� (�⺻�� nullptr)
	bool BindParam(SQLUSMALLINT paramIndex, SQLSMALLINT cType, SQLSMALLINT sqlType,
		SQLULEN len, SQLPOINTER ptr, SQLLEN* index = nullptr);

	// ��� �÷��� C++ ������ ����
	// index�� ���� ���̸� ���� �� ���� (�⺻�� nullptr)
	bool BindCol(SQLUSMALLINT columnIndex, SQLSMALLINT cType,
		SQLULEN len, SQLPOINTER value, SQLLEN* index = nullptr);

	// SQL ���� �� �߻��� ���� ���
	void HandleError(SQLRETURN ret);

private:
	// DB ���� �ڵ�
	SQLHDBC _connection = SQL_NULL_HANDLE;

	// Statement �ڵ� (Prepare/Execute/Fetch ���� ���⿡)
	SQLHSTMT _statement = SQL_NULL_HANDLE;
};

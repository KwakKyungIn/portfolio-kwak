#pragma once
#include <sql.h>
#include <sqlext.h>

class DBConnection
{
public:
	// DB 연결 시도 (환경 핸들과 연결 문자열 필요)
	bool Connect(SQLHENV henv, const WCHAR* connectionString);

	// 연결/statement 정리
	void Clear();

	// 쿼리 준비 (PreparedStatement 용)
	bool Prepare(const WCHAR* query);

	// 준비된 쿼리 실행
	bool Execute();

	// 쿼리 직접 실행 (Prepare 없이 바로)
	bool Execute(const WCHAR* query);

	// 결과셋에서 다음 행 불러오기
	bool Fetch();

	// 실행된 쿼리의 결과 행 개수 확인
	int32 GetRowCount();

	// statement 상태 초기화 (커서 닫기, 바인딩 해제)
	void Unbind();

public:
	// 쿼리 파라미터 바인딩 (INSERT, UPDATE 등에 사용)
	// index는 출력 파라미터 길이를 담을 수 있음 (기본값 nullptr)
	bool BindParam(SQLUSMALLINT paramIndex, SQLSMALLINT cType, SQLSMALLINT sqlType,
		SQLULEN len, SQLPOINTER ptr, SQLLEN* index = nullptr);

	// 결과 컬럼을 C++ 변수에 매핑
	// index는 실제 길이를 담을 수 있음 (기본값 nullptr)
	bool BindCol(SQLUSMALLINT columnIndex, SQLSMALLINT cType,
		SQLULEN len, SQLPOINTER value, SQLLEN* index = nullptr);

	// SQL 실행 시 발생한 에러 출력
	void HandleError(SQLRETURN ret);

private:
	// DB 연결 핸들
	SQLHDBC _connection = SQL_NULL_HANDLE;

	// Statement 핸들 (Prepare/Execute/Fetch 전부 여기에)
	SQLHSTMT _statement = SQL_NULL_HANDLE;
};

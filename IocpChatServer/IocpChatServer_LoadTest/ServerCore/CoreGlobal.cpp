#include "pch.h"
#include "CoreGlobal.h"
#include "ThreadManager.h"
#include "Memory.h"
#include "DeadLockProfiler.h"
#include "SocketUtils.h"
#include "SendBuffer.h"
#include "DBConnectionPool.h"
#include "MetricsRunner.h"

// 전역 싱글톤 인스턴스들
ThreadManager* GThreadManager = nullptr;
Memory* GMemory = nullptr;
SendBufferManager* GSendBufferManager = nullptr;
DeadLockProfiler* GDeadLockProfiler = nullptr;
DBConnectionPool* GDBConnectionPool = nullptr;

// CoreGlobal: 서버 전체에서 공용으로 사용하는 전역 객체들을 초기화/정리
class CoreGlobal
{
public:
	CoreGlobal()
	{
		// 핵심 매니저 객체들 초기화
		GThreadManager = new ThreadManager();
		GMemory = new Memory();
		GSendBufferManager = new SendBufferManager();
		GDeadLockProfiler = new DeadLockProfiler();
		GDBConnectionPool = new DBConnectionPool();

		// 메트릭스 주기적 수집 시작
		StartMetricsTicker();

		// 소켓 라이브러리 초기화
		SocketUtils::Init();

		cout << "CoreGlobal Initialized" << endl;
	}

	~CoreGlobal()
	{
		// 객체 메모리 해제
		delete GThreadManager;
		delete GMemory;
		delete GSendBufferManager;
		delete GDeadLockProfiler;
		delete GDBConnectionPool;

		// 메트릭스 수집 종료
		StopMetricsTicker();

		// 소켓 라이브러리 정리
		SocketUtils::Clear();
	}
} GCoreGlobal; // 전역 CoreGlobal 인스턴스 (프로그램 시작 시 자동 실행)

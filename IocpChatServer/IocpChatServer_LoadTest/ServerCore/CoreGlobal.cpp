#include "pch.h"
#include "CoreGlobal.h"
#include "ThreadManager.h"
#include "Memory.h"
#include "DeadLockProfiler.h"
#include "SocketUtils.h"
#include "SendBuffer.h"
#include "DBConnectionPool.h"
#include "MetricsRunner.h"

// ���� �̱��� �ν��Ͻ���
ThreadManager* GThreadManager = nullptr;
Memory* GMemory = nullptr;
SendBufferManager* GSendBufferManager = nullptr;
DeadLockProfiler* GDeadLockProfiler = nullptr;
DBConnectionPool* GDBConnectionPool = nullptr;

// CoreGlobal: ���� ��ü���� �������� ����ϴ� ���� ��ü���� �ʱ�ȭ/����
class CoreGlobal
{
public:
	CoreGlobal()
	{
		// �ٽ� �Ŵ��� ��ü�� �ʱ�ȭ
		GThreadManager = new ThreadManager();
		GMemory = new Memory();
		GSendBufferManager = new SendBufferManager();
		GDeadLockProfiler = new DeadLockProfiler();
		GDBConnectionPool = new DBConnectionPool();

		// ��Ʈ���� �ֱ��� ���� ����
		StartMetricsTicker();

		// ���� ���̺귯�� �ʱ�ȭ
		SocketUtils::Init();

		cout << "CoreGlobal Initialized" << endl;
	}

	~CoreGlobal()
	{
		// ��ü �޸� ����
		delete GThreadManager;
		delete GMemory;
		delete GSendBufferManager;
		delete GDeadLockProfiler;
		delete GDBConnectionPool;

		// ��Ʈ���� ���� ����
		StopMetricsTicker();

		// ���� ���̺귯�� ����
		SocketUtils::Clear();
	}
} GCoreGlobal; // ���� CoreGlobal �ν��Ͻ� (���α׷� ���� �� �ڵ� ����)

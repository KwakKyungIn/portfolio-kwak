#pragma once
#include "NetAddress.h"
#include "IocpCore.h"
#include "Listener.h"
#include <functional>

// ==============================
// ServiceType
// - � Ÿ���� �������� ���� (Server/Client)
// ==============================
enum class ServiceType : uint8
{
	Server,
	Client
};

// ���� ���� ���丮 (�ݹ�)
// - ��ü���� Session Ÿ���� �ܺο��� ����
using SessionFactory = function<SessionRef(void)>;

// ==============================
// Service
// - ����/Ŭ�� ���� ��� Ŭ����
// - ���� ����, ����, ��ε�ĳ��Ʈ ���
// - enable_shared_from_this �� �ڱ� �ڽ� ���� ����
// ==============================
class Service : public enable_shared_from_this<Service>
{
public:
	Service(ServiceType type, NetAddress address, IocpCoreRef core, SessionFactory factory, int32 maxSessionCount = 1);
	virtual ~Service();

	// ���� ���� (���� ���� �� Server/Client�� ����)
	virtual bool Start() abstract;

	// ���� ���丮�� ��ȿ���� �˻�
	bool CanStart() { return _sessionFactory != nullptr; }

	// ���� ���� (���� �Լ�)
	virtual void CloseService();

	// ���� ���丮 ��ü
	void SetSessionFactory(SessionFactory func) { _sessionFactory = func; }

	// ��� ���ǿ� ��ε�ĳ��Ʈ
	void Broadcast(SendBufferRef sendBuffer);

	// ���� ���� (���丮 + IOCP ���)
	SessionRef CreateSession();

	// ���� ��� (���� ���� ��)
	void AddSession(SessionRef session);

	// ���� ���� (���� ���� ��)
	void ReleaseSession(SessionRef session);

	// ���� ���� ��
	int32 GetCurrentSessionCount() { return _sessionCount; }

	// �ִ� ���� ��
	int32 GetMaxSessionCount() { return _maxSessionCount; }

public:
	// ���� Ÿ�� (Server/Client)
	ServiceType GetServiceType() { return _type; }

	// ������ ��Ʈ��ũ �ּ�
	NetAddress GetNetAddress() { return _netAddress; }

	// IOCP Core ���� ��ȯ
	IocpCoreRef& GetIocpCore() { return _iocpCore; }

protected:
	USE_LOCK;                 // ���� �����̳� ��ȣ�� ��
	ServiceType _type;        // ���� Ÿ��
	NetAddress _netAddress;   // ���� �ּ�(IP, Port)
	IocpCoreRef _iocpCore;    // IOCP Core �ڵ�

	Set<SessionRef> _sessions;      // ���� ���� ���� ���� ���
	int32 _sessionCount = 0;        // ���� ���� ��
	int32 _maxSessionCount = 0;     // �ִ� ���� ��
	SessionFactory _sessionFactory; // ���� ���� �ݹ�
};

// ==============================
// ClientService
// - Ŭ���̾�Ʈ ���� ����
// - N���� ������ ����� Connect �õ�
// ==============================
class ClientService : public Service
{
public:
	ClientService(NetAddress targetAddress, IocpCoreRef core, SessionFactory factory, int32 maxSessionCount = 1);
	virtual ~ClientService() {}

	virtual bool Start() override;
};

// ==============================
// ServerService
// - ���� ���� ����
// - Listener �����Ͽ� AcceptEx ���� ����
// ==============================
class ServerService : public Service
{
public:
	ServerService(NetAddress targetAddress, IocpCoreRef core, SessionFactory factory, int32 maxSessionCount = 1);
	virtual ~ServerService() {}

	virtual bool Start() override;
	virtual void CloseService() override;

private:
	ListenerRef _listener = nullptr; // AcceptEx ���� ������
};

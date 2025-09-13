#pragma once
#include "IocpCore.h"
#include "IocpEvent.h"
#include "NetAddress.h"
#include "RecvBuffer.h"

class Service;

// ==============================
// Session
// - IOCP ��� TCP ���� ����
// - Send/Recv ���������ΰ� �̺�Ʈ ó�� ���
// ==============================
class Session : public IocpObject
{
	friend class Listener;
	friend class IocpCore;
	friend class Service;

	enum
	{
		BUFFER_SIZE = 0x10000, // RecvBuffer ũ��: 64KB
	};

public:
	Session();
	virtual ~Session();

	// -------- �ܺ� API --------
	void Send(SendBufferRef sendBuffer);       // ������ �۽� ��û
	bool Connect();                            // Ŭ���̾�Ʈ ��� ����
	void Disconnect(const WCHAR* cause);       // ���� ����

	shared_ptr<Service> GetService() { return _service.lock(); }
	void SetService(shared_ptr<Service> service) { _service = service; }

	// -------- ���� ���� --------
	void SetNetAddress(NetAddress address) { _netAddress = address; }
	NetAddress GetAddress() { return _netAddress; }
	SOCKET GetSocket() { return _socket; }
	bool IsConnected() { return _connected; }
	SessionRef GetSessionRef() { return static_pointer_cast<Session>(shared_from_this()); }

	// -------- �۽� ��å ��� --------
	static constexpr int32 MAX_SEND_BATCH_BYTES = 32 * 1024;   // WSASend�� �ִ� 32KB
	static constexpr int32 MAX_SEND_BATCH_COUNT = 64;          // WSASend�� �ִ� 64�� ����

	static constexpr int64 MAX_BACKLOG_BYTES = 128 * 1024;     // �۽� ��⿭ �ϵ�ĸ 128KB
	static constexpr int32 MAX_BACKLOG_COUNT = 256;            // �۽� ��⿭ �ϵ�ĸ 256��

	// �̹� WSASend ��� ���̸� ����Ʈĸ ����
	static constexpr int64 SOFT_BACKLOG_BYTES_WHEN_REGISTERED = 64 * 1024;
	static constexpr int32 SOFT_BACKLOG_COUNT_WHEN_REGISTERED = 128;

private:
	// IOCP �������̽� ����
	virtual HANDLE GetHandle() override;
	virtual void Dispatch(class IocpEvent* iocpEvent, int32 numOfBytes = 0) override;

	// -------- ���� ó�� --------
	bool RegisterConnect();     // ConnectEx ���
	bool RegisterDisconnect();  // DisconnectEx ���
	void RegisterRecv();        // WSARecv ���
	void RegisterSend();        // WSASend ���

	void ProcessConnect();      // IOCP Connect �Ϸ� ó��
	void ProcessDisconnect();   // IOCP Disconnect �Ϸ� ó��
	void ProcessRecv(int32 numOfBytes); // IOCP Recv �Ϸ� ó��
	void ProcessSend(int32 numOfBytes); // IOCP Send �Ϸ� ó��

	void HandleError(int32 errorCode); // ���� ���� ó��

protected:
	// ������ �� (��ӹ޾� ����)
	virtual void OnConnected() {}
	virtual int32 OnRecv(BYTE* buffer, int32 len) { return len; }
	virtual void OnSend(int32 len) {}
	virtual void OnDisconnected() {}

private:
	weak_ptr<Service> _service;     // �Ҽӵ� ���� (Server/Client)
	SOCKET _socket = INVALID_SOCKET;// ���� ���� �ڵ�
	NetAddress _netAddress = {};    // ���� �ּ�
	Atomic<bool> _connected = false;// ���� ����

private:
	USE_LOCK;

	// -------- ���� ���� --------
	RecvBuffer _recvBuffer; // 64KB �⺻ ����

	// -------- �۽� ���� --------
	Queue<SendBufferRef> _sendQueue;   // �۽� ��⿭
	Atomic<bool> _sendRegistered = false; // WSASend ��� ����

	int64 _sendBacklogBytes = 0; // ��� ���� �� ����Ʈ
	int32 _sendBacklogCount = 0; // ��� ���� ���� ����

private:
	// IOCP �̺�Ʈ ��ü ����
	ConnectEvent _connectEvent;
	DisconnectEvent _disconnectEvent;
	RecvEvent _recvEvent;
	SendEvent _sendEvent;
};

// ==============================
// PacketSession
// - ��Ŷ ���� Recv ����
// - [size(2)][id(2)][data...] ����
// ==============================
struct PacketHeader
{
	uint16 size;
	uint16 id; // �������� ID (ex. 1=�α���, 2=�̵� ��û ��)
};

class PacketSession : public Session
{
public:
	PacketSession();
	virtual ~PacketSession();

	PacketSessionRef GetPacketSessionRef() { return static_pointer_cast<PacketSession>(shared_from_this()); }

protected:
	// RecvBuffer �� ��Ŷ ���� �Ľ�
	virtual int32 OnRecv(BYTE* buffer, int32 len) sealed;

	// ���� ��Ŷ ó�� (��� Ŭ�������� ����)
	virtual void OnRecvPacket(BYTE* buffer, int32 len) abstract;
};

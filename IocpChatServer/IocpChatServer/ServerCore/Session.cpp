#include "pch.h"
#include "Session.h"
#include "SocketUtils.h"
#include "Service.h"

// ==============================
// Session
// - �ϳ��� TCP ���� ������ ����
// - IOCP �̺�Ʈ(Connect/Disconnect/Recv/Send)�� ó��
// - �ۼ��� ť�� ��α�(��⿭) ������ ������ Ȯ��
// ==============================

Session::Session() : _recvBuffer(BUFFER_SIZE)
{
	// �� ���� ���� (TCP, Overlapped I/O)
	_socket = SocketUtils::CreateSocket();
}

Session::~Session()
{
	// �Ҹ� �� ���� ����
	SocketUtils::Close(_socket);
}

// ------------------------------
// Send() 
// - �ܺο��� ��Ŷ�� ���� �� ȣ��
// - �۽� ť�� �װ�, �ʿ�� RegisterSend() �ߵ�
// - �ϵ�/����Ʈ ĸ���� ���� ��ȣ
// ------------------------------
void Session::Send(SendBufferRef sendBuffer)
{
	if (IsConnected() == false)
		return;

	bool registerSend = false;
	const int32 sz = static_cast<int32>(sendBuffer->WriteSize());

	{
		WRITE_LOCK;

		// [�ϵ�ĸ] ������ �۽� ��⿭�� �����ϰ� ���̸� ���
		if ((_sendBacklogBytes + sz > MAX_BACKLOG_BYTES) ||
			(_sendBacklogCount + 1 > MAX_BACKLOG_COUNT))
		{
			// �ʿ��ϴٸ� ���⼭ Disconnect()�� ���� ���� ����
			return; // ���� ��å: ���
		}

		// [����Ʈĸ] �̹� WSASend ���� ���̶�� �� ���� �Ӱ�ġ ����
		if (_sendRegistered.load(std::memory_order_relaxed))
		{
			if ((_sendBacklogBytes >= SOFT_BACKLOG_BYTES_WHEN_REGISTERED) ||
				(_sendBacklogCount >= SOFT_BACKLOG_COUNT_WHEN_REGISTERED))
			{
				return; // ���
			}
		}

		// �۽� ť�� �ױ�
		_sendQueue.push(sendBuffer);
		_sendBacklogBytes += sz;
		_sendBacklogCount += 1;

		// ���� WSASend�� ��� ���� �ƴϾ��ٸ�, �̹��� ��� �ʿ�
		if (_sendRegistered.exchange(true) == false)
			registerSend = true;
	}

	// ���� WSASend ����� �� �ٱ����� ����
	if (registerSend)
		RegisterSend();
}

// ------------------------------
// Connect() : Ŭ���̾�Ʈ ��� ���� �õ�
// ------------------------------
bool Session::Connect()
{
	return RegisterConnect();
}

// ------------------------------
// Disconnect()
// - �ߺ� ���� (������ �÷���)
// - DisconnectEx ���
// ------------------------------
void Session::Disconnect(const WCHAR* cause)
{
	if (_connected.exchange(false) == false)
		return;

	// TEMP �α�
	wcout << "Disconnect : " << cause << endl;

	RegisterDisconnect();
}

// IOCP �������̽� ����
HANDLE Session::GetHandle()
{
	return reinterpret_cast<HANDLE>(_socket);
}

// IOCP �Ϸ� ���� �� �ش� �̺�Ʈ ó��
void Session::Dispatch(IocpEvent* iocpEvent, int32 numOfBytes)
{
	switch (iocpEvent->eventType)
	{
	case EventType::Connect:    ProcessConnect();    break;
	case EventType::Disconnect: ProcessDisconnect(); break;
	case EventType::Recv:       ProcessRecv(numOfBytes); break;
	case EventType::Send:       ProcessSend(numOfBytes); break;
	default: break;
	}
}

// ------------------------------
// RegisterConnect()
// - Ŭ���̾�Ʈ ���ǿ����� ���
// - ConnectEx�� �񵿱� ���� �õ�
// ------------------------------
bool Session::RegisterConnect()
{
	if (IsConnected())
		return false;
	if (GetService()->GetServiceType() != ServiceType::Client)
		return false;

	// ���� �ɼ�
	if (SocketUtils::SetReuseAddress(_socket, true) == false) return false;
	if (SocketUtils::BindAnyAddress(_socket, 0) == false) return false;

	_connectEvent.Init();
	_connectEvent.owner = shared_from_this(); // ADD_REF

	DWORD numOfBytes = 0;
	SOCKADDR_IN sockAddr = GetService()->GetNetAddress().GetSockAddr();

	if (false == SocketUtils::ConnectEx(
		_socket,
		reinterpret_cast<SOCKADDR*>(&sockAddr),
		sizeof(sockAddr),
		nullptr, 0,
		&numOfBytes,
		&_connectEvent))
	{
		int32 errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			_connectEvent.owner = nullptr; // RELEASE_REF
			return false;
		}
	}
	return true;
}

// ------------------------------
// RegisterDisconnect()
// - DisconnectEx�� �񵿱� ����
// - TF_REUSE_SOCKET �ɼ�: ���� ���� ����
// ------------------------------
bool Session::RegisterDisconnect()
{
	_disconnectEvent.Init();
	_disconnectEvent.owner = shared_from_this(); // ADD_REF

	if (false == SocketUtils::DisconnectEx(_socket, &_disconnectEvent, TF_REUSE_SOCKET, 0))
	{
		int32 errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			_disconnectEvent.owner = nullptr; // RELEASE_REF
			return false;
		}
	}
	return true;
}

// ------------------------------
// RegisterRecv()
// - RecvEvent �غ� �� WSARecv �񵿱� ��û
// ------------------------------
void Session::RegisterRecv()
{
	if (IsConnected() == false)
		return;

	_recvEvent.Init();
	_recvEvent.owner = shared_from_this(); // ADD_REF

	WSABUF wsaBuf;
	wsaBuf.buf = reinterpret_cast<char*>(_recvBuffer.WritePos());
	wsaBuf.len = _recvBuffer.FreeSize();

	DWORD numOfBytes = 0;
	DWORD flags = 0;
	if (SOCKET_ERROR == ::WSARecv(_socket, &wsaBuf, 1, OUT & numOfBytes, OUT & flags, &_recvEvent, nullptr))
	{
		int32 errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			HandleError(errorCode);
			_recvEvent.owner = nullptr; // RELEASE_REF
		}
	}
}

// ------------------------------
// RegisterSend()
// - �۽� ť���� ������(batch) ���� WSASend ���
// - Scatter-Gather WSASend���� ���� �ش�ȭ
// ------------------------------
void Session::RegisterSend()
{
	if (IsConnected() == false)
		return;

	_sendEvent.Init();
	_sendEvent.owner = shared_from_this(); // ADD_REF

	// ��ġ �۾�
	{
		WRITE_LOCK;

		int32 batchedBytes = 0;
		while (_sendQueue.empty() == false)
		{
			SendBufferRef sb = _sendQueue.front();
			const int32 sz = static_cast<int32>(sb->WriteSize());

			// ù ��°�� ������ ����, ���Ĵ� ���Ѽ� üũ
			if (!_sendEvent.sendBuffers.empty())
			{
				if (batchedBytes + sz > MAX_SEND_BATCH_BYTES) break;
				if ((int32)_sendEvent.sendBuffers.size() >= MAX_SEND_BATCH_COUNT) break;
			}

			_sendQueue.pop();
			_sendEvent.sendBuffers.push_back(sb);
			batchedBytes += sz;

			// ��α� ��� ����
			_sendBacklogBytes -= sz;
			_sendBacklogCount -= 1;
		}
	}

	// Scatter-Gather WSASend �غ�
	Vector<WSABUF> wsaBufs;
	wsaBufs.reserve(_sendEvent.sendBuffers.size());
	for (SendBufferRef sb : _sendEvent.sendBuffers)
	{
		WSABUF b;
		b.buf = reinterpret_cast<char*>(sb->Buffer());
		b.len = static_cast<ULONG>(sb->WriteSize());
		wsaBufs.push_back(b);
	}

	DWORD numOfBytes = 0;
	if (SOCKET_ERROR == ::WSASend(_socket, wsaBufs.data(), (DWORD)wsaBufs.size(), OUT & numOfBytes, 0, &_sendEvent, nullptr))
	{
		const int32 err = ::WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			HandleError(err);
			_sendEvent.owner = nullptr;     // RELEASE_REF
			_sendEvent.sendBuffers.clear(); // RELEASE_REF
			_sendRegistered.store(false);   // �ٽ� �õ� ����
		}
	}
}

// ------------------------------
// ProcessConnect()
// - IOCP Connect �Ϸ� ó��
// ------------------------------
void Session::ProcessConnect()
{
	_connectEvent.owner = nullptr; // RELEASE_REF
	_connected.store(true);

	// Service�� ���� ���
	GetService()->AddSession(GetSessionRef());

	// ������ ��
	OnConnected();

	// ù Recv ���
	RegisterRecv();
}

// ------------------------------
// ProcessDisconnect()
// - IOCP Disconnect �Ϸ� ó��
// ------------------------------
void Session::ProcessDisconnect()
{
	_disconnectEvent.owner = nullptr; // RELEASE_REF

	OnDisconnected(); // ������ ��
	GetService()->ReleaseSession(GetSessionRef());
}

// ------------------------------
// ProcessRecv()
// - IOCP Recv �Ϸ� ó��
// - ���ۿ� ������ ��� �� ������ OnRecv ȣ��
// - �ҷ� ������/Ŀ�� ���� �� Disconnect
// ------------------------------
void Session::ProcessRecv(int32 numOfBytes)
{
	_recvEvent.owner = nullptr; // RELEASE_REF

	if (numOfBytes == 0)
	{
		Disconnect(L"Recv 0");
		return;
	}

	if (_recvBuffer.OnWrite(numOfBytes) == false)
	{
		Disconnect(L"OnWrite Overflow");
		return;
	}

	int32 dataSize = _recvBuffer.DataSize();
	int32 processLen = OnRecv(_recvBuffer.ReadPos(), dataSize); // ������ ��
	if (processLen < 0 || dataSize < processLen || _recvBuffer.OnRead(processLen) == false)
	{
		Disconnect(L"OnRead Overflow");
		return;
	}

	// Ŀ�� ���� (������ ����)
	_recvBuffer.Clean();

	// �ٽ� Recv ��� �� ���� ����
	RegisterRecv();
}

// ------------------------------
// ProcessSend()
// - IOCP Send �Ϸ� ó��
// - �۽� ť�� ���� �� ������ ����
// ------------------------------
void Session::ProcessSend(int32 numOfBytes)
{
	_sendEvent.owner = nullptr;      // RELEASE_REF
	_sendEvent.sendBuffers.clear();  // RELEASE_REF

	if (numOfBytes == 0)
	{
		Disconnect(L"Send 0");
		return;
	}

	OnSend(numOfBytes); // ������ ��

	// �۽� ť�� ���� �� ������ �ٽ� ���
	WRITE_LOCK;
	if (_sendQueue.empty())
		_sendRegistered.store(false);
	else
		RegisterSend();
}

// ------------------------------
// HandleError()
// - ���� ���� �߻� �� ó��
// ------------------------------
void Session::HandleError(int32 errorCode)
{
	switch (errorCode)
	{
	case WSAECONNRESET:
	case WSAECONNABORTED:
		Disconnect(L"HandleError");
		break;
	default:
		// TODO : �α� �����
		cout << "Handle Error : " << errorCode << endl;
		break;
	}
}

// ==============================
// PacketSession
// - Session�� ��ӹ޾� ��Ŷ ���� ó�� ����
// - �⺻ ����: [size(2)][id(2)][data...]
// ==============================

PacketSession::PacketSession()
{
}

PacketSession::~PacketSession()
{
}

// OnRecv()
// - RecvBuffer�� ������ �����͸� ��Ŷ ������ �Ľ�
// - �ϼ��� ��Ŷ ������ OnRecvPacket ȣ��
int32 PacketSession::OnRecv(BYTE* buffer, int32 len)
{
	int32 processLen = 0;

	while (true)
	{
		int32 dataSize = len - processLen;

		// ������� �� ������ ���
		if (dataSize < sizeof(PacketHeader))
			break;

		PacketHeader header = *(reinterpret_cast<PacketHeader*>(&buffer[processLen]));

		// ��Ŷ ��ü�� ���� �� �޾����� ���
		if (dataSize < header.size)
			break;

		// ��Ŷ ���� ���� �� ������ �� ȣ��
		OnRecvPacket(&buffer[processLen], header.size);

		processLen += header.size;
	}

	return processLen;
}

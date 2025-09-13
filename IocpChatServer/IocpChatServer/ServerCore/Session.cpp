#include "pch.h"
#include "Session.h"
#include "SocketUtils.h"
#include "Service.h"

// ==============================
// Session
// - 하나의 TCP 연결 단위를 관리
// - IOCP 이벤트(Connect/Disconnect/Recv/Send)를 처리
// - 송수신 큐와 백로그(대기열) 관리로 안정성 확보
// ==============================

Session::Session() : _recvBuffer(BUFFER_SIZE)
{
	// 새 소켓 생성 (TCP, Overlapped I/O)
	_socket = SocketUtils::CreateSocket();
}

Session::~Session()
{
	// 소멸 시 소켓 정리
	SocketUtils::Close(_socket);
}

// ------------------------------
// Send() 
// - 외부에서 패킷을 보낼 때 호출
// - 송신 큐에 쌓고, 필요시 RegisterSend() 발동
// - 하드/소프트 캡으로 세션 보호
// ------------------------------
void Session::Send(SendBufferRef sendBuffer)
{
	if (IsConnected() == false)
		return;

	bool registerSend = false;
	const int32 sz = static_cast<int32>(sendBuffer->WriteSize());

	{
		WRITE_LOCK;

		// [하드캡] 세션의 송신 대기열이 과도하게 쌓이면 드롭
		if ((_sendBacklogBytes + sz > MAX_BACKLOG_BYTES) ||
			(_sendBacklogCount + 1 > MAX_BACKLOG_COUNT))
		{
			// 필요하다면 여기서 Disconnect()로 강제 종료 가능
			return; // 현재 정책: 드롭
		}

		// [소프트캡] 이미 WSASend 진행 중이라면 더 낮은 임계치 적용
		if (_sendRegistered.load(std::memory_order_relaxed))
		{
			if ((_sendBacklogBytes >= SOFT_BACKLOG_BYTES_WHEN_REGISTERED) ||
				(_sendBacklogCount >= SOFT_BACKLOG_COUNT_WHEN_REGISTERED))
			{
				return; // 드롭
			}
		}

		// 송신 큐에 쌓기
		_sendQueue.push(sendBuffer);
		_sendBacklogBytes += sz;
		_sendBacklogCount += 1;

		// 현재 WSASend가 등록 중이 아니었다면, 이번에 등록 필요
		if (_sendRegistered.exchange(true) == false)
			registerSend = true;
	}

	// 실제 WSASend 등록은 락 바깥에서 수행
	if (registerSend)
		RegisterSend();
}

// ------------------------------
// Connect() : 클라이언트 모드 연결 시도
// ------------------------------
bool Session::Connect()
{
	return RegisterConnect();
}

// ------------------------------
// Disconnect()
// - 중복 방지 (원자적 플래그)
// - DisconnectEx 등록
// ------------------------------
void Session::Disconnect(const WCHAR* cause)
{
	if (_connected.exchange(false) == false)
		return;

	// TEMP 로그
	wcout << "Disconnect : " << cause << endl;

	RegisterDisconnect();
}

// IOCP 인터페이스 구현
HANDLE Session::GetHandle()
{
	return reinterpret_cast<HANDLE>(_socket);
}

// IOCP 완료 통지 → 해당 이벤트 처리
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
// - 클라이언트 세션에서만 사용
// - ConnectEx로 비동기 연결 시도
// ------------------------------
bool Session::RegisterConnect()
{
	if (IsConnected())
		return false;
	if (GetService()->GetServiceType() != ServiceType::Client)
		return false;

	// 소켓 옵션
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
// - DisconnectEx로 비동기 해제
// - TF_REUSE_SOCKET 옵션: 소켓 재사용 가능
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
// - RecvEvent 준비 후 WSARecv 비동기 요청
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
// - 송신 큐에서 일정량(batch) 꺼내 WSASend 등록
// - Scatter-Gather WSASend으로 성능 극대화
// ------------------------------
void Session::RegisterSend()
{
	if (IsConnected() == false)
		return;

	_sendEvent.Init();
	_sendEvent.owner = shared_from_this(); // ADD_REF

	// 배치 작업
	{
		WRITE_LOCK;

		int32 batchedBytes = 0;
		while (_sendQueue.empty() == false)
		{
			SendBufferRef sb = _sendQueue.front();
			const int32 sz = static_cast<int32>(sb->WriteSize());

			// 첫 번째는 무조건 포함, 이후는 상한선 체크
			if (!_sendEvent.sendBuffers.empty())
			{
				if (batchedBytes + sz > MAX_SEND_BATCH_BYTES) break;
				if ((int32)_sendEvent.sendBuffers.size() >= MAX_SEND_BATCH_COUNT) break;
			}

			_sendQueue.pop();
			_sendEvent.sendBuffers.push_back(sb);
			batchedBytes += sz;

			// 백로그 장부 차감
			_sendBacklogBytes -= sz;
			_sendBacklogCount -= 1;
		}
	}

	// Scatter-Gather WSASend 준비
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
			_sendRegistered.store(false);   // 다시 시도 가능
		}
	}
}

// ------------------------------
// ProcessConnect()
// - IOCP Connect 완료 처리
// ------------------------------
void Session::ProcessConnect()
{
	_connectEvent.owner = nullptr; // RELEASE_REF
	_connected.store(true);

	// Service에 세션 등록
	GetService()->AddSession(GetSessionRef());

	// 컨텐츠 훅
	OnConnected();

	// 첫 Recv 등록
	RegisterRecv();
}

// ------------------------------
// ProcessDisconnect()
// - IOCP Disconnect 완료 처리
// ------------------------------
void Session::ProcessDisconnect()
{
	_disconnectEvent.owner = nullptr; // RELEASE_REF

	OnDisconnected(); // 컨텐츠 훅
	GetService()->ReleaseSession(GetSessionRef());
}

// ------------------------------
// ProcessRecv()
// - IOCP Recv 완료 처리
// - 버퍼에 데이터 기록 → 컨텐츠 OnRecv 호출
// - 불량 데이터/커서 오류 시 Disconnect
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
	int32 processLen = OnRecv(_recvBuffer.ReadPos(), dataSize); // 컨텐츠 훅
	if (processLen < 0 || dataSize < processLen || _recvBuffer.OnRead(processLen) == false)
	{
		Disconnect(L"OnRead Overflow");
		return;
	}

	// 커서 정리 (앞으로 당기기)
	_recvBuffer.Clean();

	// 다시 Recv 등록 → 루프 유지
	RegisterRecv();
}

// ------------------------------
// ProcessSend()
// - IOCP Send 완료 처리
// - 송신 큐에 남은 게 있으면 재등록
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

	OnSend(numOfBytes); // 컨텐츠 훅

	// 송신 큐에 남은 게 있으면 다시 등록
	WRITE_LOCK;
	if (_sendQueue.empty())
		_sendRegistered.store(false);
	else
		RegisterSend();
}

// ------------------------------
// HandleError()
// - 소켓 오류 발생 시 처리
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
		// TODO : 로그 남기기
		cout << "Handle Error : " << errorCode << endl;
		break;
	}
}

// ==============================
// PacketSession
// - Session을 상속받아 패킷 단위 처리 지원
// - 기본 구조: [size(2)][id(2)][data...]
// ==============================

PacketSession::PacketSession()
{
}

PacketSession::~PacketSession()
{
}

// OnRecv()
// - RecvBuffer에 누적된 데이터를 패킷 단위로 파싱
// - 완성된 패킷 단위로 OnRecvPacket 호출
int32 PacketSession::OnRecv(BYTE* buffer, int32 len)
{
	int32 processLen = 0;

	while (true)
	{
		int32 dataSize = len - processLen;

		// 헤더조차 못 읽으면 대기
		if (dataSize < sizeof(PacketHeader))
			break;

		PacketHeader header = *(reinterpret_cast<PacketHeader*>(&buffer[processLen]));

		// 패킷 전체를 아직 못 받았으면 대기
		if (dataSize < header.size)
			break;

		// 패킷 조립 성공 → 컨텐츠 훅 호출
		OnRecvPacket(&buffer[processLen], header.size);

		processLen += header.size;
	}

	return processLen;
}

#pragma once
#include "IocpCore.h"
#include "IocpEvent.h"
#include "NetAddress.h"
#include "RecvBuffer.h"

class Service;

// ==============================
// Session
// - IOCP 기반 TCP 연결 단위
// - Send/Recv 파이프라인과 이벤트 처리 담당
// ==============================
class Session : public IocpObject
{
	friend class Listener;
	friend class IocpCore;
	friend class Service;

	enum
	{
		BUFFER_SIZE = 0x10000, // RecvBuffer 크기: 64KB
	};

public:
	Session();
	virtual ~Session();

	// -------- 외부 API --------
	void Send(SendBufferRef sendBuffer);       // 데이터 송신 요청
	bool Connect();                            // 클라이언트 모드 연결
	void Disconnect(const WCHAR* cause);       // 연결 종료

	shared_ptr<Service> GetService() { return _service.lock(); }
	void SetService(shared_ptr<Service> service) { _service = service; }

	// -------- 연결 정보 --------
	void SetNetAddress(NetAddress address) { _netAddress = address; }
	NetAddress GetAddress() { return _netAddress; }
	SOCKET GetSocket() { return _socket; }
	bool IsConnected() { return _connected; }
	SessionRef GetSessionRef() { return static_pointer_cast<Session>(shared_from_this()); }

	// -------- 송신 정책 상수 --------
	static constexpr int32 MAX_SEND_BATCH_BYTES = 32 * 1024;   // WSASend당 최대 32KB
	static constexpr int32 MAX_SEND_BATCH_COUNT = 64;          // WSASend당 최대 64개 버퍼

	static constexpr int64 MAX_BACKLOG_BYTES = 128 * 1024;     // 송신 대기열 하드캡 128KB
	static constexpr int32 MAX_BACKLOG_COUNT = 256;            // 송신 대기열 하드캡 256개

	// 이미 WSASend 등록 중이면 소프트캡 적용
	static constexpr int64 SOFT_BACKLOG_BYTES_WHEN_REGISTERED = 64 * 1024;
	static constexpr int32 SOFT_BACKLOG_COUNT_WHEN_REGISTERED = 128;

private:
	// IOCP 인터페이스 구현
	virtual HANDLE GetHandle() override;
	virtual void Dispatch(class IocpEvent* iocpEvent, int32 numOfBytes = 0) override;

	// -------- 내부 처리 --------
	bool RegisterConnect();     // ConnectEx 등록
	bool RegisterDisconnect();  // DisconnectEx 등록
	void RegisterRecv();        // WSARecv 등록
	void RegisterSend();        // WSASend 등록

	void ProcessConnect();      // IOCP Connect 완료 처리
	void ProcessDisconnect();   // IOCP Disconnect 완료 처리
	void ProcessRecv(int32 numOfBytes); // IOCP Recv 완료 처리
	void ProcessSend(int32 numOfBytes); // IOCP Send 완료 처리

	void HandleError(int32 errorCode); // 소켓 오류 처리

protected:
	// 컨텐츠 훅 (상속받아 구현)
	virtual void OnConnected() {}
	virtual int32 OnRecv(BYTE* buffer, int32 len) { return len; }
	virtual void OnSend(int32 len) {}
	virtual void OnDisconnected() {}

private:
	weak_ptr<Service> _service;     // 소속된 서비스 (Server/Client)
	SOCKET _socket = INVALID_SOCKET;// 실제 소켓 핸들
	NetAddress _netAddress = {};    // 원격 주소
	Atomic<bool> _connected = false;// 연결 상태

private:
	USE_LOCK;

	// -------- 수신 관련 --------
	RecvBuffer _recvBuffer; // 64KB 기본 버퍼

	// -------- 송신 관련 --------
	Queue<SendBufferRef> _sendQueue;   // 송신 대기열
	Atomic<bool> _sendRegistered = false; // WSASend 등록 여부

	int64 _sendBacklogBytes = 0; // 대기 중인 총 바이트
	int32 _sendBacklogCount = 0; // 대기 중인 버퍼 개수

private:
	// IOCP 이벤트 객체 재사용
	ConnectEvent _connectEvent;
	DisconnectEvent _disconnectEvent;
	RecvEvent _recvEvent;
	SendEvent _sendEvent;
};

// ==============================
// PacketSession
// - 패킷 단위 Recv 지원
// - [size(2)][id(2)][data...] 구조
// ==============================
struct PacketHeader
{
	uint16 size;
	uint16 id; // 프로토콜 ID (ex. 1=로그인, 2=이동 요청 등)
};

class PacketSession : public Session
{
public:
	PacketSession();
	virtual ~PacketSession();

	PacketSessionRef GetPacketSessionRef() { return static_pointer_cast<PacketSession>(shared_from_this()); }

protected:
	// RecvBuffer → 패킷 단위 파싱
	virtual int32 OnRecv(BYTE* buffer, int32 len) sealed;

	// 실제 패킷 처리 (상속 클래스에서 구현)
	virtual void OnRecvPacket(BYTE* buffer, int32 len) abstract;
};

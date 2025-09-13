#pragma once
#include "NetAddress.h"
#include "IocpCore.h"
#include "Listener.h"
#include <functional>

// ==============================
// ServiceType
// - 어떤 타입의 서비스인지 구분 (Server/Client)
// ==============================
enum class ServiceType : uint8
{
	Server,
	Client
};

// 세션 생성 팩토리 (콜백)
// - 구체적인 Session 타입은 외부에서 지정
using SessionFactory = function<SessionRef(void)>;

// ==============================
// Service
// - 서버/클라 공통 기반 클래스
// - 세션 생성, 관리, 브로드캐스트 담당
// - enable_shared_from_this → 자기 자신 참조 안전
// ==============================
class Service : public enable_shared_from_this<Service>
{
public:
	Service(ServiceType type, NetAddress address, IocpCoreRef core, SessionFactory factory, int32 maxSessionCount = 1);
	virtual ~Service();

	// 서비스 시작 (순수 가상 → Server/Client가 구현)
	virtual bool Start() abstract;

	// 세션 팩토리가 유효한지 검사
	bool CanStart() { return _sessionFactory != nullptr; }

	// 서비스 종료 (가상 함수)
	virtual void CloseService();

	// 세션 팩토리 교체
	void SetSessionFactory(SessionFactory func) { _sessionFactory = func; }

	// 모든 세션에 브로드캐스트
	void Broadcast(SendBufferRef sendBuffer);

	// 세션 생성 (팩토리 + IOCP 등록)
	SessionRef CreateSession();

	// 세션 등록 (연결 성공 시)
	void AddSession(SessionRef session);

	// 세션 해제 (연결 종료 시)
	void ReleaseSession(SessionRef session);

	// 현재 세션 수
	int32 GetCurrentSessionCount() { return _sessionCount; }

	// 최대 세션 수
	int32 GetMaxSessionCount() { return _maxSessionCount; }

public:
	// 서비스 타입 (Server/Client)
	ServiceType GetServiceType() { return _type; }

	// 서비스의 네트워크 주소
	NetAddress GetNetAddress() { return _netAddress; }

	// IOCP Core 참조 반환
	IocpCoreRef& GetIocpCore() { return _iocpCore; }

protected:
	USE_LOCK;                 // 세션 컨테이너 보호용 락
	ServiceType _type;        // 서비스 타입
	NetAddress _netAddress;   // 서비스 주소(IP, Port)
	IocpCoreRef _iocpCore;    // IOCP Core 핸들

	Set<SessionRef> _sessions;      // 현재 접속 중인 세션 목록
	int32 _sessionCount = 0;        // 현재 세션 수
	int32 _maxSessionCount = 0;     // 최대 세션 수
	SessionFactory _sessionFactory; // 세션 생성 콜백
};

// ==============================
// ClientService
// - 클라이언트 역할 서비스
// - N개의 세션을 만들고 Connect 시도
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
// - 서버 역할 서비스
// - Listener 생성하여 AcceptEx 루프 시작
// ==============================
class ServerService : public Service
{
public:
	ServerService(NetAddress targetAddress, IocpCoreRef core, SessionFactory factory, int32 maxSessionCount = 1);
	virtual ~ServerService() {}

	virtual bool Start() override;
	virtual void CloseService() override;

private:
	ListenerRef _listener = nullptr; // AcceptEx 전담 리스너
};

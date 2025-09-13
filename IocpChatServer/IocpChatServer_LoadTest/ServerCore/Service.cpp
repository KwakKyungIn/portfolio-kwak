#include "pch.h"
#include "Service.h"
#include "Session.h"
#include "Listener.h"

// ==============================
// Service
// - 서버/클라이언트 서비스의 공통 기반
// - 세션 생성/등록/브로드캐스트/해제 관리
// ==============================

Service::Service(ServiceType type, NetAddress address, IocpCoreRef core, SessionFactory factory, int32 maxSessionCount)
	: _type(type), _netAddress(address), _iocpCore(core), _sessionFactory(factory), _maxSessionCount(maxSessionCount)
{
}

Service::~Service()
{
}

// 서비스 종료 (현재는 TODO, 자원 정리 필요)
void Service::CloseService()
{
	// TODO : 모든 세션 종료, Listener 해제 등
}

// 모든 세션에 동일한 메시지 브로드캐스트
// - lock으로 세션 컨테이너 보호
void Service::Broadcast(SendBufferRef sendBuffer)
{
	WRITE_LOCK;
	for (const auto& session : _sessions)
	{
		session->Send(sendBuffer);
	}
}

// 세션 생성
// - 팩토리 함수(SessionFactory)로 실제 세션 객체를 생성
// - Service와 IOCP Core 등록까지 수행
SessionRef Service::CreateSession()
{
	SessionRef session = _sessionFactory();
	session->SetService(shared_from_this());

	if (_iocpCore->Register(session) == false)
		return nullptr;

	return session;
}

// 세션 추가 (연결 성공 시 호출)
// - sessionCount 증가 + 컨테이너에 보관
void Service::AddSession(SessionRef session)
{
	WRITE_LOCK;
	_sessionCount++;
	_sessions.insert(session);
}

// 세션 해제 (연결 종료 시 호출)
// - 반드시 컨테이너에 존재해야 함
void Service::ReleaseSession(SessionRef session)
{
	WRITE_LOCK;
	ASSERT_CRASH(_sessions.erase(session) != 0);
	_sessionCount--;
}

// ==============================
// ClientService
// - 클라이언트 전용 Service
// - 지정한 개수만큼 Connect 시도
// ==============================

ClientService::ClientService(NetAddress targetAddress, IocpCoreRef core, SessionFactory factory, int32 maxSessionCount)
	: Service(ServiceType::Client, targetAddress, core, factory, maxSessionCount)
{
}

bool ClientService::Start()
{
	if (CanStart() == false)
		return false;

	const int32 sessionCount = GetMaxSessionCount();
	for (int32 i = 0; i < sessionCount; i++)
	{
		SessionRef session = CreateSession();
		if (session->Connect() == false)
			return false;
	}

	return true;
}

// ==============================
// ServerService
// - 서버 전용 Service
// - Listener를 생성하여 AcceptEx 루프 시작
// ==============================

ServerService::ServerService(NetAddress address, IocpCoreRef core, SessionFactory factory, int32 maxSessionCount)
	: Service(ServiceType::Server, address, core, factory, maxSessionCount)
{
}

bool ServerService::Start()
{
	if (CanStart() == false)
		return false;

	// Listener 생성 및 AcceptEx 준비
	_listener = MakeShared<Listener>();
	if (_listener == nullptr)
		return false;

	// shared_from_this()로 자기 자신을 ServerServiceRef로 변환
	ServerServiceRef service = static_pointer_cast<ServerService>(shared_from_this());
	if (_listener->StartAccept(service) == false)
		return false;

	return true;
}

void ServerService::CloseService()
{
	// TODO : Listener 닫기, 세션 정리 필요
	Service::CloseService();
}

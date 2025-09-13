#include "pch.h"
#include "Listener.h"
#include "SocketUtils.h"
#include "IocpEvent.h"
#include "Session.h"
#include "Service.h"

/*--------------
    Listener
    - 서버의 리슨 소켓을 IOCP에 붙이고 AcceptEx를 지속 등록
    - Accept 완료 시 새 Session을 초기화/등록 후 다시 Accept 재무장
---------------*/

Listener::~Listener()
{
    // 리슨 소켓 정리
    SocketUtils::Close(_socket);

    // 미사용 대기 중이던 AcceptEvent들 해제
    for (AcceptEvent* acceptEvent : _acceptEvents)
    {
        // 여기서 취소(취소 IO)까지 고려하려면 CancelIoEx 등 추가 가능
        // 지금은 단순 메모리 해제만 수행
        xdelete(acceptEvent);
    }
}

bool Listener::StartAccept(ServerServiceRef service)
{
    // 서비스 보관 (세션 생성/환경 접근용)
    _service = service;
    if (_service == nullptr)
        return false;

    // 리슨 소켓 생성
    _socket = SocketUtils::CreateSocket();
    if (_socket == INVALID_SOCKET)
        return false;

    // IOCP 포트에 리슨 소켓 등록
    if (_service->GetIocpCore()->Register(shared_from_this()) == false)
        return false;

    // 소켓 옵션: 포트 재사용 허용
    if (SocketUtils::SetReuseAddress(_socket, true) == false)
        return false;

    // 소켓 옵션: LINGER(바로 종료) 설정
    if (SocketUtils::SetLinger(_socket, 0, 0) == false)
        return false;

    // 바인드 (IP:PORT)
    if (SocketUtils::Bind(_socket, _service->GetNetAddress()) == false)
        return false;

    // 리슨 상태로 전환 (큐 길이는 내부 기본)
    if (SocketUtils::Listen(_socket) == false)
        return false;

    // 초기 AcceptEx 예약 개수 = 최대 세션 수
    const int32 acceptCount = _service->GetMaxSessionCount();
    for (int32 i = 0; i < acceptCount; i++)
    {
        // Accept 완료 알림을 받을 이벤트 객체
        AcceptEvent* acceptEvent = xnew<AcceptEvent>();
        acceptEvent->owner = shared_from_this(); // 완료 시 콜백 대상
        _acceptEvents.push_back(acceptEvent);

        // 비동기 AcceptEx 등록
        RegisterAccept(acceptEvent);
    }

    return true;
}

void Listener::CloseSocket()
{
    // 리슨 소켓만 닫음 (이미 등록된 IO는 별도 경로로 정리됨)
    SocketUtils::Close(_socket);
}

HANDLE Listener::GetHandle()
{
    // IOCP에 등록될 Win32 HANDLE (SOCKET → HANDLE 캐스팅)
    return reinterpret_cast<HANDLE>(_socket);
}

void Listener::Dispatch(IocpEvent* iocpEvent, int32 numOfBytes)
{
    // Listener는 Accept 이벤트만 처리
    ASSERT_CRASH(iocpEvent->eventType == EventType::Accept);
    AcceptEvent* acceptEvent = static_cast<AcceptEvent*>(iocpEvent);

    // Accept 완료 후 처리(세션 초기화 등)
    ProcessAccept(acceptEvent);
}

void Listener::RegisterAccept(AcceptEvent* acceptEvent)
{
    // 서비스에서 새 세션 생성 (세션 내부 소켓/IOCP 등록 포함)
    SessionRef session = _service->CreateSession(); // Register IOCP

    // 이벤트 재활용을 위한 초기화
    acceptEvent->Init();
    acceptEvent->session = session; // 완료 시 넘겨줄 대상 세션

    // AcceptEx는 주소 정보 저장을 위해 추가 버퍼를 요구
    // - session->_recvBuffer.WritePos(): 수신 버퍼의 현재 쓰기 위치
    // - addrlen = sizeof(SOCKADDR_IN) + 16 (Local/Remote 각각)
    DWORD bytesReceived = 0;
    if (false == SocketUtils::AcceptEx(
        _socket,
        session->GetSocket(),
        session->_recvBuffer.WritePos(), // 연결 직후 Pre-Recv 없이 바로 WSABUF 채우기 가능
        0,                               // dataLen=0 (데이터는 안 받음, 주소만)
        sizeof(SOCKADDR_IN) + 16,        // local addr
        sizeof(SOCKADDR_IN) + 16,        // remote addr
        OUT & bytesReceived,
        static_cast<LPOVERLAPPED>(acceptEvent)))
    {
        // 비동기 착수 실패 시
        const int32 errorCode = ::WSAGetLastError();
        if (errorCode != WSA_IO_PENDING)
        {
            // 즉시 실패한 케이스 → 다시 등록해서 수용률 유지
            RegisterAccept(acceptEvent);
        }
    }
}

void Listener::ProcessAccept(AcceptEvent* acceptEvent)
{
    // Accept 완료된 세션
    SessionRef session = acceptEvent->session;

    // Accept된 소켓에 리슨 소켓 옵션 상속 (SO_UPDATE_ACCEPT_CONTEXT)
    if (false == SocketUtils::SetUpdateAcceptSocket(session->GetSocket(), _socket))
    {
        // 실패 시 해당 AcceptEvent를 재무장해서 다음 연결 대기
        RegisterAccept(acceptEvent);
        return;
    }

    // 원격 클라이언트 주소 얻기 (표시/로깅용)
    SOCKADDR_IN sockAddress;
    int32 sizeOfSockAddr = sizeof(sockAddress);
    if (SOCKET_ERROR == ::getpeername(session->GetSocket(), OUT reinterpret_cast<SOCKADDR*>(&sockAddress), &sizeOfSockAddr))
    {
        // 주소 조회 실패해도 Accept 루프는 유지
        RegisterAccept(acceptEvent);
        return;
    }

    // 세션에 원격 주소 셋업
    session->SetNetAddress(NetAddress(sockAddress));

    // 세션 연결 후크 호출 (상태 플래그/초기 Recv 등록 등)
    session->ProcessConnect();

    // 마지막으로 동일 AcceptEvent를 다시 등록 → 지속 수용
    RegisterAccept(acceptEvent);
}

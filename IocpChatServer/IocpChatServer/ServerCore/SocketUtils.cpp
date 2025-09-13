#include "pch.h"
#include "SocketUtils.h"

// ==============================
// SocketUtils
// - WinSock API 래퍼 유틸리티
// - Overlapped I/O용 확장 함수(ConnectEx 등)도
//   런타임에 동적으로 바인딩해야 해서 별도 관리
// ==============================

// 확장 함수 포인터 (런타임에 바인딩됨)
LPFN_CONNECTEX      SocketUtils::ConnectEx = nullptr;
LPFN_DISCONNECTEX   SocketUtils::DisconnectEx = nullptr;
LPFN_ACCEPTEX       SocketUtils::AcceptEx = nullptr;

// ------------------------------
// Init()
// - WinSock 라이브러리 초기화
// - ConnectEx / DisconnectEx / AcceptEx 주소 바인딩
// ------------------------------
void SocketUtils::Init()
{
    WSADATA wsaData;
    ASSERT_CRASH(::WSAStartup(MAKEWORD(2, 2), OUT & wsaData) == 0);

    // Overlapped 확장 API는 소켓별로 가져와야 함
    SOCKET dummySocket = CreateSocket();
    ASSERT_CRASH(BindWindowsFunction(dummySocket, WSAID_CONNECTEX, reinterpret_cast<LPVOID*>(&ConnectEx)));
    ASSERT_CRASH(BindWindowsFunction(dummySocket, WSAID_DISCONNECTEX, reinterpret_cast<LPVOID*>(&DisconnectEx)));
    ASSERT_CRASH(BindWindowsFunction(dummySocket, WSAID_ACCEPTEX, reinterpret_cast<LPVOID*>(&AcceptEx)));
    Close(dummySocket);
}

// ------------------------------
// Clear()
// - WinSock 종료 (WSAStartup 카운트 감소)
// ------------------------------
void SocketUtils::Clear()
{
    ::WSACleanup();
}

// ------------------------------
// BindWindowsFunction()
// - 확장 함수 주소를 얻는 헬퍼
// - SIO_GET_EXTENSION_FUNCTION_POINTER 사용
// ------------------------------
bool SocketUtils::BindWindowsFunction(SOCKET socket, GUID guid, LPVOID* fn)
{
    DWORD bytes = 0;
    return SOCKET_ERROR != ::WSAIoctl(
        socket,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guid, sizeof(guid),
        fn, sizeof(*fn),
        OUT & bytes,
        NULL, NULL);
}

// ------------------------------
// CreateSocket()
// - Overlapped I/O를 위한 소켓 생성
// - WSA_FLAG_OVERLAPPED 플래그 중요
// ------------------------------
SOCKET SocketUtils::CreateSocket()
{
    return ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
}

// ------------------------------
// SetLinger()
// - 소켓 close() 시 처리 방식 결정
// - onoff=1 & linger>0 → 데이터 다 보내고 종료
// - onoff=0 → 바로 종료
// ------------------------------
bool SocketUtils::SetLinger(SOCKET socket, uint16 onoff, uint16 linger)
{
    LINGER option;
    option.l_onoff = onoff;
    option.l_linger = linger;
    return SetSockOpt(socket, SOL_SOCKET, SO_LINGER, option);
}

// ------------------------------
// SetReuseAddress()
// - TIME_WAIT 상태의 포트 재사용 허용
// ------------------------------
bool SocketUtils::SetReuseAddress(SOCKET socket, bool flag)
{
    return SetSockOpt(socket, SOL_SOCKET, SO_REUSEADDR, flag);
}

// ------------------------------
// SetRecvBufferSize() / SetSendBufferSize()
// - 커널 소켓 버퍼 크기 조절
// ------------------------------
bool SocketUtils::SetRecvBufferSize(SOCKET socket, int32 size)
{
    return SetSockOpt(socket, SOL_SOCKET, SO_RCVBUF, size);
}

bool SocketUtils::SetSendBufferSize(SOCKET socket, int32 size)
{
    return SetSockOpt(socket, SOL_SOCKET, SO_SNDBUF, size);
}

// ------------------------------
// SetTcpNoDelay()
// - Nagle 알고리즘 끄기 (소량 패킷 지연 방지)
// - true = 바로바로 송신
// ------------------------------
bool SocketUtils::SetTcpNoDelay(SOCKET socket, bool flag)
{
    return SetSockOpt(socket, SOL_SOCKET, TCP_NODELAY, flag);
}

// ------------------------------
// SetUpdateAcceptSocket()
// - AcceptEx로 생성된 소켓에
//   ListenSocket의 옵션을 복사
// - 클라이언트 소켓도 정상 동작 가능하게 함
// ------------------------------
bool SocketUtils::SetUpdateAcceptSocket(SOCKET socket, SOCKET listenSocket)
{
    return SetSockOpt(socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, listenSocket);
}

// ------------------------------
// Bind()
// - 특정 IP/Port에 소켓 바인딩
// ------------------------------
bool SocketUtils::Bind(SOCKET socket, NetAddress netAddr)
{
    return SOCKET_ERROR != ::bind(socket,
        reinterpret_cast<const SOCKADDR*>(&netAddr.GetSockAddr()),
        sizeof(SOCKADDR_IN));
}

// ------------------------------
// BindAnyAddress()
// - INADDR_ANY로 모든 인터페이스 바인딩
// - 서버 Listen 소켓에서 주로 사용
// ------------------------------
bool SocketUtils::BindAnyAddress(SOCKET socket, uint16 port)
{
    SOCKADDR_IN myAddress;
    myAddress.sin_family = AF_INET;
    myAddress.sin_addr.s_addr = ::htonl(INADDR_ANY);
    myAddress.sin_port = ::htons(port);

    return SOCKET_ERROR != ::bind(socket,
        reinterpret_cast<const SOCKADDR*>(&myAddress),
        sizeof(myAddress));
}

// ------------------------------
// Listen()
// - 서버 Listen 상태 진입
// - backlog = 연결 대기열 크기
// ------------------------------
bool SocketUtils::Listen(SOCKET socket, int32 backlog)
{
    return SOCKET_ERROR != ::listen(socket, backlog);
}

// ------------------------------
// Close()
// - 소켓 안전하게 닫기
// - INVALID_SOCKET으로 초기화
// ------------------------------
void SocketUtils::Close(SOCKET& socket)
{
    if (socket != INVALID_SOCKET)
        ::closesocket(socket);
    socket = INVALID_SOCKET;
}

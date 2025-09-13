#include "pch.h"
#include "SocketUtils.h"

// ==============================
// SocketUtils
// - WinSock API ���� ��ƿ��Ƽ
// - Overlapped I/O�� Ȯ�� �Լ�(ConnectEx ��)��
//   ��Ÿ�ӿ� �������� ���ε��ؾ� �ؼ� ���� ����
// ==============================

// Ȯ�� �Լ� ������ (��Ÿ�ӿ� ���ε���)
LPFN_CONNECTEX      SocketUtils::ConnectEx = nullptr;
LPFN_DISCONNECTEX   SocketUtils::DisconnectEx = nullptr;
LPFN_ACCEPTEX       SocketUtils::AcceptEx = nullptr;

// ------------------------------
// Init()
// - WinSock ���̺귯�� �ʱ�ȭ
// - ConnectEx / DisconnectEx / AcceptEx �ּ� ���ε�
// ------------------------------
void SocketUtils::Init()
{
    WSADATA wsaData;
    ASSERT_CRASH(::WSAStartup(MAKEWORD(2, 2), OUT & wsaData) == 0);

    // Overlapped Ȯ�� API�� ���Ϻ��� �����;� ��
    SOCKET dummySocket = CreateSocket();
    ASSERT_CRASH(BindWindowsFunction(dummySocket, WSAID_CONNECTEX, reinterpret_cast<LPVOID*>(&ConnectEx)));
    ASSERT_CRASH(BindWindowsFunction(dummySocket, WSAID_DISCONNECTEX, reinterpret_cast<LPVOID*>(&DisconnectEx)));
    ASSERT_CRASH(BindWindowsFunction(dummySocket, WSAID_ACCEPTEX, reinterpret_cast<LPVOID*>(&AcceptEx)));
    Close(dummySocket);
}

// ------------------------------
// Clear()
// - WinSock ���� (WSAStartup ī��Ʈ ����)
// ------------------------------
void SocketUtils::Clear()
{
    ::WSACleanup();
}

// ------------------------------
// BindWindowsFunction()
// - Ȯ�� �Լ� �ּҸ� ��� ����
// - SIO_GET_EXTENSION_FUNCTION_POINTER ���
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
// - Overlapped I/O�� ���� ���� ����
// - WSA_FLAG_OVERLAPPED �÷��� �߿�
// ------------------------------
SOCKET SocketUtils::CreateSocket()
{
    return ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
}

// ------------------------------
// SetLinger()
// - ���� close() �� ó�� ��� ����
// - onoff=1 & linger>0 �� ������ �� ������ ����
// - onoff=0 �� �ٷ� ����
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
// - TIME_WAIT ������ ��Ʈ ���� ���
// ------------------------------
bool SocketUtils::SetReuseAddress(SOCKET socket, bool flag)
{
    return SetSockOpt(socket, SOL_SOCKET, SO_REUSEADDR, flag);
}

// ------------------------------
// SetRecvBufferSize() / SetSendBufferSize()
// - Ŀ�� ���� ���� ũ�� ����
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
// - Nagle �˰��� ���� (�ҷ� ��Ŷ ���� ����)
// - true = �ٷιٷ� �۽�
// ------------------------------
bool SocketUtils::SetTcpNoDelay(SOCKET socket, bool flag)
{
    return SetSockOpt(socket, SOL_SOCKET, TCP_NODELAY, flag);
}

// ------------------------------
// SetUpdateAcceptSocket()
// - AcceptEx�� ������ ���Ͽ�
//   ListenSocket�� �ɼ��� ����
// - Ŭ���̾�Ʈ ���ϵ� ���� ���� �����ϰ� ��
// ------------------------------
bool SocketUtils::SetUpdateAcceptSocket(SOCKET socket, SOCKET listenSocket)
{
    return SetSockOpt(socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, listenSocket);
}

// ------------------------------
// Bind()
// - Ư�� IP/Port�� ���� ���ε�
// ------------------------------
bool SocketUtils::Bind(SOCKET socket, NetAddress netAddr)
{
    return SOCKET_ERROR != ::bind(socket,
        reinterpret_cast<const SOCKADDR*>(&netAddr.GetSockAddr()),
        sizeof(SOCKADDR_IN));
}

// ------------------------------
// BindAnyAddress()
// - INADDR_ANY�� ��� �������̽� ���ε�
// - ���� Listen ���Ͽ��� �ַ� ���
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
// - ���� Listen ���� ����
// - backlog = ���� ��⿭ ũ��
// ------------------------------
bool SocketUtils::Listen(SOCKET socket, int32 backlog)
{
    return SOCKET_ERROR != ::listen(socket, backlog);
}

// ------------------------------
// Close()
// - ���� �����ϰ� �ݱ�
// - INVALID_SOCKET���� �ʱ�ȭ
// ------------------------------
void SocketUtils::Close(SOCKET& socket)
{
    if (socket != INVALID_SOCKET)
        ::closesocket(socket);
    socket = INVALID_SOCKET;
}

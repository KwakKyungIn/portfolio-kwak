#pragma once
#include "NetAddress.h"

// ==============================
// SocketUtils
// - WinSock API 래퍼 클래스
// - 소켓 생성, 옵션 설정, 확장 API 바인딩 등
// ==============================
class SocketUtils
{
public:
    // 런타임 확장 함수 포인터
    static LPFN_CONNECTEX     ConnectEx;
    static LPFN_DISCONNECTEX  DisconnectEx;
    static LPFN_ACCEPTEX      AcceptEx;

public:
    static void Init();    // WinSock 초기화 + 확장 함수 바인딩
    static void Clear();   // WinSock 정리 (WSACleanup)

    static bool BindWindowsFunction(SOCKET socket, GUID guid, LPVOID* fn);
    static SOCKET CreateSocket();

    // 소켓 옵션 설정
    static bool SetLinger(SOCKET socket, uint16 onoff, uint16 linger);
    static bool SetReuseAddress(SOCKET socket, bool flag);
    static bool SetRecvBufferSize(SOCKET socket, int32 size);
    static bool SetSendBufferSize(SOCKET socket, int32 size);
    static bool SetTcpNoDelay(SOCKET socket, bool flag);
    static bool SetUpdateAcceptSocket(SOCKET socket, SOCKET listenSocket);

    // 바인딩 / 리스닝 / 종료
    static bool Bind(SOCKET socket, NetAddress netAddr);
    static bool BindAnyAddress(SOCKET socket, uint16 port);
    static bool Listen(SOCKET socket, int32 backlog = SOMAXCONN);
    static void Close(SOCKET& socket);
};

// ------------------------------
// SetSockOpt<T>
// - 템플릿으로 묶어서 옵션 설정 공통화
// ------------------------------
template<typename T>
static inline bool SetSockOpt(SOCKET socket, int32 level, int32 optName, T optVal)
{
    return SOCKET_ERROR != ::setsockopt(
        socket, level, optName,
        reinterpret_cast<char*>(&optVal),
        sizeof(T));
}

#include "pch.h"
#include "Listener.h"
#include "SocketUtils.h"
#include "IocpEvent.h"
#include "Session.h"
#include "Service.h"

/*--------------
    Listener
    - ������ ���� ������ IOCP�� ���̰� AcceptEx�� ���� ���
    - Accept �Ϸ� �� �� Session�� �ʱ�ȭ/��� �� �ٽ� Accept �繫��
---------------*/

Listener::~Listener()
{
    // ���� ���� ����
    SocketUtils::Close(_socket);

    // �̻�� ��� ���̴� AcceptEvent�� ����
    for (AcceptEvent* acceptEvent : _acceptEvents)
    {
        // ���⼭ ���(��� IO)���� ����Ϸ��� CancelIoEx �� �߰� ����
        // ������ �ܼ� �޸� ������ ����
        xdelete(acceptEvent);
    }
}

bool Listener::StartAccept(ServerServiceRef service)
{
    // ���� ���� (���� ����/ȯ�� ���ٿ�)
    _service = service;
    if (_service == nullptr)
        return false;

    // ���� ���� ����
    _socket = SocketUtils::CreateSocket();
    if (_socket == INVALID_SOCKET)
        return false;

    // IOCP ��Ʈ�� ���� ���� ���
    if (_service->GetIocpCore()->Register(shared_from_this()) == false)
        return false;

    // ���� �ɼ�: ��Ʈ ���� ���
    if (SocketUtils::SetReuseAddress(_socket, true) == false)
        return false;

    // ���� �ɼ�: LINGER(�ٷ� ����) ����
    if (SocketUtils::SetLinger(_socket, 0, 0) == false)
        return false;

    // ���ε� (IP:PORT)
    if (SocketUtils::Bind(_socket, _service->GetNetAddress()) == false)
        return false;

    // ���� ���·� ��ȯ (ť ���̴� ���� �⺻)
    if (SocketUtils::Listen(_socket) == false)
        return false;

    // �ʱ� AcceptEx ���� ���� = �ִ� ���� ��
    const int32 acceptCount = _service->GetMaxSessionCount();
    for (int32 i = 0; i < acceptCount; i++)
    {
        // Accept �Ϸ� �˸��� ���� �̺�Ʈ ��ü
        AcceptEvent* acceptEvent = xnew<AcceptEvent>();
        acceptEvent->owner = shared_from_this(); // �Ϸ� �� �ݹ� ���
        _acceptEvents.push_back(acceptEvent);

        // �񵿱� AcceptEx ���
        RegisterAccept(acceptEvent);
    }

    return true;
}

void Listener::CloseSocket()
{
    // ���� ���ϸ� ���� (�̹� ��ϵ� IO�� ���� ��η� ������)
    SocketUtils::Close(_socket);
}

HANDLE Listener::GetHandle()
{
    // IOCP�� ��ϵ� Win32 HANDLE (SOCKET �� HANDLE ĳ����)
    return reinterpret_cast<HANDLE>(_socket);
}

void Listener::Dispatch(IocpEvent* iocpEvent, int32 numOfBytes)
{
    // Listener�� Accept �̺�Ʈ�� ó��
    ASSERT_CRASH(iocpEvent->eventType == EventType::Accept);
    AcceptEvent* acceptEvent = static_cast<AcceptEvent*>(iocpEvent);

    // Accept �Ϸ� �� ó��(���� �ʱ�ȭ ��)
    ProcessAccept(acceptEvent);
}

void Listener::RegisterAccept(AcceptEvent* acceptEvent)
{
    // ���񽺿��� �� ���� ���� (���� ���� ����/IOCP ��� ����)
    SessionRef session = _service->CreateSession(); // Register IOCP

    // �̺�Ʈ ��Ȱ���� ���� �ʱ�ȭ
    acceptEvent->Init();
    acceptEvent->session = session; // �Ϸ� �� �Ѱ��� ��� ����

    // AcceptEx�� �ּ� ���� ������ ���� �߰� ���۸� �䱸
    // - session->_recvBuffer.WritePos(): ���� ������ ���� ���� ��ġ
    // - addrlen = sizeof(SOCKADDR_IN) + 16 (Local/Remote ����)
    DWORD bytesReceived = 0;
    if (false == SocketUtils::AcceptEx(
        _socket,
        session->GetSocket(),
        session->_recvBuffer.WritePos(), // ���� ���� Pre-Recv ���� �ٷ� WSABUF ä��� ����
        0,                               // dataLen=0 (�����ʹ� �� ����, �ּҸ�)
        sizeof(SOCKADDR_IN) + 16,        // local addr
        sizeof(SOCKADDR_IN) + 16,        // remote addr
        OUT & bytesReceived,
        static_cast<LPOVERLAPPED>(acceptEvent)))
    {
        // �񵿱� ���� ���� ��
        const int32 errorCode = ::WSAGetLastError();
        if (errorCode != WSA_IO_PENDING)
        {
            // ��� ������ ���̽� �� �ٽ� ����ؼ� ����� ����
            RegisterAccept(acceptEvent);
        }
    }
}

void Listener::ProcessAccept(AcceptEvent* acceptEvent)
{
    // Accept �Ϸ�� ����
    SessionRef session = acceptEvent->session;

    // Accept�� ���Ͽ� ���� ���� �ɼ� ��� (SO_UPDATE_ACCEPT_CONTEXT)
    if (false == SocketUtils::SetUpdateAcceptSocket(session->GetSocket(), _socket))
    {
        // ���� �� �ش� AcceptEvent�� �繫���ؼ� ���� ���� ���
        RegisterAccept(acceptEvent);
        return;
    }

    // ���� Ŭ���̾�Ʈ �ּ� ��� (ǥ��/�α��)
    SOCKADDR_IN sockAddress;
    int32 sizeOfSockAddr = sizeof(sockAddress);
    if (SOCKET_ERROR == ::getpeername(session->GetSocket(), OUT reinterpret_cast<SOCKADDR*>(&sockAddress), &sizeOfSockAddr))
    {
        // �ּ� ��ȸ �����ص� Accept ������ ����
        RegisterAccept(acceptEvent);
        return;
    }

    // ���ǿ� ���� �ּ� �¾�
    session->SetNetAddress(NetAddress(sockAddress));

    // ���� ���� ��ũ ȣ�� (���� �÷���/�ʱ� Recv ��� ��)
    session->ProcessConnect();

    // ���������� ���� AcceptEvent�� �ٽ� ��� �� ���� ����
    RegisterAccept(acceptEvent);
}

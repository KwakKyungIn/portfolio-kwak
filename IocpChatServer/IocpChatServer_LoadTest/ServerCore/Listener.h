#pragma once
#include "IocpCore.h"
#include "NetAddress.h"

class AcceptEvent;
class ServerService;

/*--------------
    Listener
    - ���� ������ �����ϰ� IOCP�� ���
    - AcceptEx�� N�� ������Ͽ� ���� ����(accept pipeline)
    - �Ϸ� �� �� Session �ʱ�ȭ �� �ٽ� Accept �繫��
---------------*/
class Listener : public IocpObject
{
public:
    Listener() = default;
    ~Listener();

public:
    // �ܺ� API: ���� ����
    // - service: ���� ����/ȯ�� ����(������, �ּ�, �ھ� ��)
    // - ���ο��� CreateSocket/Bind/Listen/IOCP ���/AcceptEx N�� ����
    bool StartAccept(ServerServiceRef service);

    // �ܺ� API: ���� ���ϸ� �ݱ�
    void CloseSocket();

public:
    // IocpObject �������̽�
    virtual HANDLE GetHandle() override;                                   // IOCP�� ��ϵ� HANDLE
    virtual void   Dispatch(class IocpEvent* iocpEvent, int32 numOfBytes = 0) override; // Accept �Ϸ� �ݹ�

private:
    // ����: AcceptEx ��� (�񵿱� ����)
    void RegisterAccept(AcceptEvent* acceptEvent);

    // ����: Accept �Ϸ� ó�� (�ɼ� ���/peer �ּ�/���� Ȱ��ȭ)
    void ProcessAccept(AcceptEvent* acceptEvent);

protected:
    // ������ ���� �ڵ�
    SOCKET _socket = INVALID_SOCKET;

    // ���� ����� Accept �̺�Ʈ Ǯ (����)
    Vector<AcceptEvent*> _acceptEvents;

    // ���� ����/ȯ�� ������ ���� ���� �ڵ�
    ServerServiceRef _service;
};

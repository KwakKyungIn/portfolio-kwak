#pragma once

/*----------------
    IocpObject
    - IOCP�� ��ϵǴ� ��� ��ü�� ���� �������̽�
    - ��ü�� HANDLE ���� + �Ϸ� ���� �� ���� ����
-----------------*/
class IocpObject : public enable_shared_from_this<IocpObject>
{
public:
    // IOCP�� ����� Win32 HANDLE
    virtual HANDLE GetHandle() abstract;

    // IOCP �Ϸ� ���� �� ȣ��Ǵ� �ݹ�
    // - iocpEvent: � ������ �̺�Ʈ���� (Recv/Send/Accept ��)
    // - numOfBytes: �Ϸ�� ����Ʈ ��(Recv/Send���� ���)
    virtual void Dispatch(class IocpEvent* iocpEvent, int32 numOfBytes = 0) abstract;
};

/*--------------
    IocpCore
    - IOCP ��Ʈ�� �����ϰ�, IocpObject�� ���/����ġ
    - ������ �������� Dispatch(timeout) ȣ��� �̺�Ʈ ó��
---------------*/
class IocpCore
{
public:
    IocpCore();
    ~IocpCore();

    // ��Ʈ �ڵ� (�ʿ� �� �ܺο��� ��ȸ)
    HANDLE GetHandle() { return _iocpHandle; }

    // HANDLE�� IOCP ��Ʈ�� ����
    bool Register(IocpObjectRef iocpObject);

    // �Ϸ� ť���� �̺�Ʈ �ϳ� ������ �й�
    // - true: ���� ó����
    // - false: Ÿ�Ӿƿ� ������ ó���� �̺�Ʈ�� ����
    bool Dispatch(uint32 timeoutMs = INFINITE);

private:
    HANDLE _iocpHandle; // IOCP ��Ʈ �ڵ�
};

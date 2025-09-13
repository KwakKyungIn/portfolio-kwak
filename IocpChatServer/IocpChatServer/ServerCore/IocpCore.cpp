#include "pch.h"
#include "IocpCore.h"
#include "IocpEvent.h"

/*--------------
    IocpCore
    - IOCP ��Ʈ ����/����
    - ��ü ���(Register) �� �Ϸ� ����(Dispatch) ó��
---------------*/

IocpCore::IocpCore()
{
    // IOCP ��Ʈ ���� (������ ��=0 �� �ý����� ������ ������ �����ٸ�)
    _iocpHandle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
    ASSERT_CRASH(_iocpHandle != INVALID_HANDLE_VALUE);
}

IocpCore::~IocpCore()
{
    // ��Ʈ �ڵ� ����
    ::CloseHandle(_iocpHandle);
}

bool IocpCore::Register(IocpObjectRef iocpObject)
{
    // ��ü�� HANDLE�� IOCP ��Ʈ�� ����
    // key�� 0 ���(���� Ű ������ �ʿ� ���� ��)
    return ::CreateIoCompletionPort(iocpObject->GetHandle(), _iocpHandle, /*key*/0, 0);
}

bool IocpCore::Dispatch(uint32 timeoutMs)
{
    // �Ϸ� ���� ����� ���� ������
    DWORD      numOfBytes = 0;      // �Ϸ�� ����Ʈ ��
    ULONG_PTR  key = 0;             // CreateIoCompletionPort���� �� key
    IocpEvent* iocpEvent = nullptr; // OVERLAPPED �� IocpEvent*

    // �Ϸ� ť���� �ϳ� ���� (timeoutMs ���� ���)
    if (::GetQueuedCompletionStatus(_iocpHandle, OUT & numOfBytes, OUT & key, OUT reinterpret_cast<LPOVERLAPPED*>(&iocpEvent), timeoutMs))
    {
        // ���� �Ϸ�: �̺�Ʈ�� owner(IocpObject)���� �й�
        IocpObjectRef iocpObject = iocpEvent->owner;
        iocpObject->Dispatch(iocpEvent, numOfBytes);
    }
    else
    {
        // ����(�Ǵ� �Ϻ� ���̽�) ó��
        int32 errCode = ::WSAGetLastError();
        switch (errCode)
        {
        case WAIT_TIMEOUT:
            // Ÿ�Ӿƿ�: ó���� �̺�Ʈ ����
            return false;
        default:
            // ���������� OVERLAPPED�� ��ȿ�� �� ���� �� �����ϰ� Dispatch
            // (��밡 ����ٵ��� ���� ������ �� ��ü���� ó��)
            // TODO: �ʿ� �� ���� �α�
            IocpObjectRef iocpObject = iocpEvent->owner;
            iocpObject->Dispatch(iocpEvent, numOfBytes);
            break;
        }
    }

    return true; // �̺�Ʈ �ϳ� ó����
}

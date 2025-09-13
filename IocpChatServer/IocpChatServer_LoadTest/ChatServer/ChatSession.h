#pragma once
#include "Session.h"
#include "Player.h"

class ChatSession : public PacketSession
{
public:
    ~ChatSession()
    {
        // 디버깅용: 세션 소멸 시 로그 출력
        cout << "~ChatSession" << endl;
    }

    // 세션 상태 변화 관련 콜백들
    virtual void OnConnected() override;                  // 연결 성공 시
    virtual void OnDisconnected() override;               // 연결 종료 시
    virtual void OnRecvPacket(BYTE* buffer, int32 len) override; // 패킷 수신 시
    virtual void OnSend(int32 len) override;              // 전송 완료 시

public:
    // 세션에는 플레이어 하나만 붙는다
    PlayerRef GetPlayer() { return _player; }
    void SetPlayer(PlayerRef player) { _player = player; }

private:
    PlayerRef _player; // 이 세션에 연결된 플레이어 정보
};

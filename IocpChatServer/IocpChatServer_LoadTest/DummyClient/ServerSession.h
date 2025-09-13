#pragma once
#include "pch.h"                 // [1] 항상 최상단
#include "Session.h"
#include "ServerPacketHandler.h"
#include "Protocol.pb.h"
#include <string>
#include <atomic>

// [클라 한 명]에 해당하는 세션 상태 + 콜백 처리
// - name: 로그인 시 서버에 보낼 닉네임
// - isLoggedIn/isInRoom/roomId: 현재 접속/입장 상태를 로컬에서 유지
class ServerSession : public PacketSession
{
public:
    explicit ServerSession(const std::string& name_ = "") : name(name_) {}

    // per-client state (테스트/디버깅용 상태값들)
    std::string name;
    std::atomic<bool> isLoggedIn{ false };
    std::atomic<bool> isInRoom{ false };
    std::atomic<int32> roomId{ -1 };

    // OnConnected: 서버 연결 직후 1회 호출
    // - 첫 메시지로 로그인 요청을 보냄 (핸드셰이크 느낌)
    void OnConnected() override
    {
        Protocol::C_LOGIN_REQ pkt;
        pkt.set_name(name);
        Send(ServerPacketHandler::MakeSendBuffer(pkt)); // 디스패처가 헤더/직렬화 처리
    }

    // OnRecvPacket: 수신 버퍼에서 완성된 패킷 단위로 호출됨
    // - ServerPacketHandler::HandlePacket이 헤더 id를 보고 적절한 핸들러 실행
    void OnRecvPacket(BYTE* buffer, int32 len) override
    {
        auto self = GetPacketSessionRef();
        ServerPacketHandler::HandlePacket(self, buffer, len);
    }

    // 연결 종료 시 상태 초기화 (재접속 시를 대비)
    void OnDisconnected() override
    {
        isLoggedIn = false;
        isInRoom = false;
        roomId = -1;
    }
};

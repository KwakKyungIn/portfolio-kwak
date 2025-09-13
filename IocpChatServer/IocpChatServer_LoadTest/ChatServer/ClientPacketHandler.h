#pragma once
#include "Protocol.pb.h"

using PacketHandlerFunc = std::function<bool(PacketSessionRef&, BYTE*, int32)>;
extern PacketHandlerFunc GPacketHandler[UINT16_MAX];

enum : uint16
{
	PKT_C_LOGIN_REQ = 1000,
	PKT_S_LOGIN_RES = 1001,
	PKT_C_CREATE_ROOM_REQ = 1002,
	PKT_S_CREATE_ROOM_RES = 1003,
	PKT_C_JOIN_ROOM_REQ = 1004,
	PKT_S_JOIN_ROOM_RES = 1005,
	PKT_S_JOIN_ROOM_NTF = 1006,
	PKT_C_ROOM_CHAT_REQ = 1007,
	PKT_S_ROOM_CHAT_NTF = 1008,
	PKT_C_LEAVE_ROOM_REQ = 1009,
	PKT_S_LEAVE_ROOM_ACK = 1010,
	PKT_S_PRESENCE_NTF = 1011,
	PKT_S_RATE_LIMIT_NTF = 1012,
	PKT_C_ADMIN_COMMAND_REQ = 1013,
	PKT_S_ADMIN_COMMAND_RES = 1014,
};

// Custom Handlers
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len);
bool Handle_C_LOGIN_REQ(PacketSessionRef& session, Protocol::C_LOGIN_REQ& pkt);
bool Handle_C_CREATE_ROOM_REQ(PacketSessionRef& session, Protocol::C_CREATE_ROOM_REQ& pkt);
bool Handle_C_JOIN_ROOM_REQ(PacketSessionRef& session, Protocol::C_JOIN_ROOM_REQ& pkt);
bool Handle_C_ROOM_CHAT_REQ(PacketSessionRef& session, Protocol::C_ROOM_CHAT_REQ& pkt);
bool Handle_C_LEAVE_ROOM_REQ(PacketSessionRef& session, Protocol::C_LEAVE_ROOM_REQ& pkt);
bool Handle_C_ADMIN_COMMAND_REQ(PacketSessionRef& session, Protocol::C_ADMIN_COMMAND_REQ& pkt);

class ClientPacketHandler
{
public:
	static void Init()
	{
		for (int32 i = 0; i < UINT16_MAX; i++)
			GPacketHandler[i] = Handle_INVALID;
		GPacketHandler[PKT_C_LOGIN_REQ] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::C_LOGIN_REQ>(Handle_C_LOGIN_REQ, session, buffer, len); };
		GPacketHandler[PKT_C_CREATE_ROOM_REQ] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::C_CREATE_ROOM_REQ>(Handle_C_CREATE_ROOM_REQ, session, buffer, len); };
		GPacketHandler[PKT_C_JOIN_ROOM_REQ] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::C_JOIN_ROOM_REQ>(Handle_C_JOIN_ROOM_REQ, session, buffer, len); };
		GPacketHandler[PKT_C_ROOM_CHAT_REQ] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::C_ROOM_CHAT_REQ>(Handle_C_ROOM_CHAT_REQ, session, buffer, len); };
		GPacketHandler[PKT_C_LEAVE_ROOM_REQ] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::C_LEAVE_ROOM_REQ>(Handle_C_LEAVE_ROOM_REQ, session, buffer, len); };
		GPacketHandler[PKT_C_ADMIN_COMMAND_REQ] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::C_ADMIN_COMMAND_REQ>(Handle_C_ADMIN_COMMAND_REQ, session, buffer, len); };
	}

	static bool HandlePacket(PacketSessionRef& session, BYTE* buffer, int32 len)
	{
		PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
		return GPacketHandler[header->id](session, buffer, len);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_LOGIN_RES& pkt) { return MakeSendBuffer(pkt, PKT_S_LOGIN_RES); }
	static SendBufferRef MakeSendBuffer(Protocol::S_CREATE_ROOM_RES& pkt) { return MakeSendBuffer(pkt, PKT_S_CREATE_ROOM_RES); }
	static SendBufferRef MakeSendBuffer(Protocol::S_JOIN_ROOM_RES& pkt) { return MakeSendBuffer(pkt, PKT_S_JOIN_ROOM_RES); }
	static SendBufferRef MakeSendBuffer(Protocol::S_JOIN_ROOM_NTF& pkt) { return MakeSendBuffer(pkt, PKT_S_JOIN_ROOM_NTF); }
	static SendBufferRef MakeSendBuffer(Protocol::S_ROOM_CHAT_NTF& pkt) { return MakeSendBuffer(pkt, PKT_S_ROOM_CHAT_NTF); }
	static SendBufferRef MakeSendBuffer(Protocol::S_LEAVE_ROOM_ACK& pkt) { return MakeSendBuffer(pkt, PKT_S_LEAVE_ROOM_ACK); }
	static SendBufferRef MakeSendBuffer(Protocol::S_PRESENCE_NTF& pkt) { return MakeSendBuffer(pkt, PKT_S_PRESENCE_NTF); }
	static SendBufferRef MakeSendBuffer(Protocol::S_RATE_LIMIT_NTF& pkt) { return MakeSendBuffer(pkt, PKT_S_RATE_LIMIT_NTF); }
	static SendBufferRef MakeSendBuffer(Protocol::S_ADMIN_COMMAND_RES& pkt) { return MakeSendBuffer(pkt, PKT_S_ADMIN_COMMAND_RES); }

private:
	template<typename PacketType, typename ProcessFunc>
	static bool HandlePacket(ProcessFunc func, PacketSessionRef& session, BYTE* buffer, int32 len)
	{
		PacketType pkt;
		if (pkt.ParseFromArray(buffer + sizeof(PacketHeader), len - sizeof(PacketHeader)) == false)
			return false;

		return func(session, pkt);
	}

	template<typename T>
	static SendBufferRef MakeSendBuffer(T& pkt, uint16 pktId)
	{
		const uint16 dataSize = static_cast<uint16>(pkt.ByteSizeLong());
		const uint16 packetSize = dataSize + sizeof(PacketHeader);

		SendBufferRef sendBuffer = GSendBufferManager->Open(packetSize);
		PacketHeader* header = reinterpret_cast<PacketHeader*>(sendBuffer->Buffer());
		header->size = packetSize;
		header->id = pktId;
		ASSERT_CRASH(pkt.SerializeToArray(&header[1], dataSize));
		sendBuffer->Close(packetSize);

		return sendBuffer;
	}
};
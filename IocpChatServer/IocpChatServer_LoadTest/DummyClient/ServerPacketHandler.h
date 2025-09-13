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
bool Handle_S_LOGIN_RES(PacketSessionRef& session, Protocol::S_LOGIN_RES& pkt);
bool Handle_S_CREATE_ROOM_RES(PacketSessionRef& session, Protocol::S_CREATE_ROOM_RES& pkt);
bool Handle_S_JOIN_ROOM_RES(PacketSessionRef& session, Protocol::S_JOIN_ROOM_RES& pkt);
bool Handle_S_JOIN_ROOM_NTF(PacketSessionRef& session, Protocol::S_JOIN_ROOM_NTF& pkt);
bool Handle_S_ROOM_CHAT_NTF(PacketSessionRef& session, Protocol::S_ROOM_CHAT_NTF& pkt);
bool Handle_S_LEAVE_ROOM_ACK(PacketSessionRef& session, Protocol::S_LEAVE_ROOM_ACK& pkt);
bool Handle_S_PRESENCE_NTF(PacketSessionRef& session, Protocol::S_PRESENCE_NTF& pkt);
bool Handle_S_RATE_LIMIT_NTF(PacketSessionRef& session, Protocol::S_RATE_LIMIT_NTF& pkt);
bool Handle_S_ADMIN_COMMAND_RES(PacketSessionRef& session, Protocol::S_ADMIN_COMMAND_RES& pkt);

class ServerPacketHandler
{
public:
	static void Init()
	{
		for (int32 i = 0; i < UINT16_MAX; i++)
			GPacketHandler[i] = Handle_INVALID;
		GPacketHandler[PKT_S_LOGIN_RES] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_LOGIN_RES>(Handle_S_LOGIN_RES, session, buffer, len); };
		GPacketHandler[PKT_S_CREATE_ROOM_RES] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_CREATE_ROOM_RES>(Handle_S_CREATE_ROOM_RES, session, buffer, len); };
		GPacketHandler[PKT_S_JOIN_ROOM_RES] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_JOIN_ROOM_RES>(Handle_S_JOIN_ROOM_RES, session, buffer, len); };
		GPacketHandler[PKT_S_JOIN_ROOM_NTF] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_JOIN_ROOM_NTF>(Handle_S_JOIN_ROOM_NTF, session, buffer, len); };
		GPacketHandler[PKT_S_ROOM_CHAT_NTF] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_ROOM_CHAT_NTF>(Handle_S_ROOM_CHAT_NTF, session, buffer, len); };
		GPacketHandler[PKT_S_LEAVE_ROOM_ACK] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_LEAVE_ROOM_ACK>(Handle_S_LEAVE_ROOM_ACK, session, buffer, len); };
		GPacketHandler[PKT_S_PRESENCE_NTF] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_PRESENCE_NTF>(Handle_S_PRESENCE_NTF, session, buffer, len); };
		GPacketHandler[PKT_S_RATE_LIMIT_NTF] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_RATE_LIMIT_NTF>(Handle_S_RATE_LIMIT_NTF, session, buffer, len); };
		GPacketHandler[PKT_S_ADMIN_COMMAND_RES] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_ADMIN_COMMAND_RES>(Handle_S_ADMIN_COMMAND_RES, session, buffer, len); };
	}

	static bool HandlePacket(PacketSessionRef& session, BYTE* buffer, int32 len)
	{
		PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
		return GPacketHandler[header->id](session, buffer, len);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_LOGIN_REQ& pkt) { return MakeSendBuffer(pkt, PKT_C_LOGIN_REQ); }
	static SendBufferRef MakeSendBuffer(Protocol::C_CREATE_ROOM_REQ& pkt) { return MakeSendBuffer(pkt, PKT_C_CREATE_ROOM_REQ); }
	static SendBufferRef MakeSendBuffer(Protocol::C_JOIN_ROOM_REQ& pkt) { return MakeSendBuffer(pkt, PKT_C_JOIN_ROOM_REQ); }
	static SendBufferRef MakeSendBuffer(Protocol::C_ROOM_CHAT_REQ& pkt) { return MakeSendBuffer(pkt, PKT_C_ROOM_CHAT_REQ); }
	static SendBufferRef MakeSendBuffer(Protocol::C_LEAVE_ROOM_REQ& pkt) { return MakeSendBuffer(pkt, PKT_C_LEAVE_ROOM_REQ); }
	static SendBufferRef MakeSendBuffer(Protocol::C_ADMIN_COMMAND_REQ& pkt) { return MakeSendBuffer(pkt, PKT_C_ADMIN_COMMAND_REQ); }

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
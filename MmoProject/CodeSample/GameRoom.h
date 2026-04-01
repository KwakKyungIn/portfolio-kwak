#pragma once
#include "JobQueue.h"
#include "Protocol.pb.h"
#include "Protocol_S2S.pb.h"
#include "SpatialGrid.h"
#include "BattleSystem.h"
#include "RoomActor.h" 

class GameMap;
class Player;
class Monster;
class Creature;
class Projectile;

class PlayerSession;
using PlayerSessionRef = std::shared_ptr<PlayerSession>;

using GameMapRef = std::shared_ptr<GameMap>;
using PlayerRef = std::shared_ptr<Player>;
using MonsterRef = std::shared_ptr<Monster>;
using ProjectileRef = std::shared_ptr<Projectile>;

// [GameRoom]
// 게임 내 하나의 "맵" 혹은 "인스턴스 던전"을 관리하는 관리자 클래스
// RoomActor를 상속받아 JobQueue를 통해 "싱글 스레드"처럼 동작함 (Lock 최소화)
// 플레이어 입장/퇴장, 전투, 이동, 아이템, 거래 등 인게임 로직의 총책임자
class GameRoom : public RoomActor, public std::enable_shared_from_this<GameRoom>
{
public:
    GameRoom();
    virtual ~GameRoom();

    // 방 생성 후 초기화 (채널, 맵 ID, 크기 등 설정)
    void Init(int32 channelId, int32 mapId, int32 sizeX, int32 sizeY, int32 zoneSize = 50);

    // 메인 루프에서 주기적으로 호출되는 업데이트 함수 (투사체 이동, 리스폰 등 처리)
    void Update();

public:
    // =========================================================
    // [RoomActor Interface]
    // RoomManager가 이 객체를 관리할 때 사용
    // =========================================================
    virtual RoomKind GetKind() const override { return RoomKind::Game; }

    // 외부에서 이 방에 일감(Job)을 던져주는 함수
    // JobQueue에 넣으면, 이 방을 담당하는 스레드가 꺼내서 처리함
    virtual void Push(std::function<void()> fn) override
    {
        _jobQueue->Push(ObjectPool<Job>::MakeShared([fn = std::move(fn)]() mutable { fn(); }));
    }

public:
    // =========================================================
    // [Job System Helpers]
    // 템플릿을 사용하여 멤버 함수나 람다를 쉽게 Job으로 포장하는 헬퍼들
    // =========================================================

    // 1. 람다 함수(Lambda)를 바로 Job으로 밀어넣을 때 사용
    template<typename F>
    void PushJob(F&& job)
    {
        _jobQueue->Push(ObjectPool<Job>::MakeShared(std::forward<F>(job)));
    }

    // 2. 특정 객체의 멤버 함수를 호출하고 싶을 때 사용
    // 예: PushJob(&GameRoom::HandleMove, this, session, pkt);
    template<typename F, typename A, typename... Args>
    void PushJob(F func, A&& arg, Args&&... args)
    {
        _jobQueue->Push(ObjectPool<Job>::MakeShared(shared_from_this(), func,
            std::forward<A>(arg), std::forward<Args>(args)...));
    }

public:
    // =========================================================
    // [Instance / Lifetime Management]
    // 인스턴스 던전 관리 및 방의 수명 주기 제어
    // =========================================================
    void SetInstanceId(int64 instanceId) { _instanceId = instanceId; }
    int64 GetInstanceId() const { return _instanceId; }

    // DB 거래 커밋 결과가 돌아왔을 때 호출 (Actor Thread에서 실행 보장되어야 함)
    void OnTradeCommitResult(Protocol::S2S_RES_TRADE_COMMIT pkt);

    // 0이면 일반 필드, 0보다 크면 인스턴스 던전
    bool IsInstanceRoom() const { return _instanceId != 0; }

    // 방 삭제 예정 플래그 설정 (플레이어가 다 나가면 삭제하기 위함)
    void MarkClosing(bool value = true) { _closing.store(value, std::memory_order_release); }
    bool IsClosing() const { return _closing.load(std::memory_order_acquire); }

    // 현재 방에 있는 대략적인 플레이어 수 (Atomic이라 정확하진 않을 수 있음)
    int32 GetPlayerCountApprox() const { return _playerCount.load(std::memory_order_acquire); }

    // 방이 비어있는지 오래되었는지 확인해서 메모리 해제 여부 결정
    // RoomManager가 주기적으로 호출함
    bool ShouldPurge(uint64 nowMs) const;

public:
    // =========================================================
    // [Content Logic - Main]
    // 실제 게임 로직들이 수행되는 곳 (반드시 JobQueue를 통해 호출됨)
    // =========================================================

    // 입장/퇴장 처리 (브로드캐스팅 포함)
    void Enter(PlayerSessionRef session, PlayerRef player);
    void Leave(PlayerSessionRef session, PlayerRef player);

    // 이동 패킷 처리 (좌표 검증 및 동기화)
    void HandleMove(PlayerSessionRef session, PlayerRef player, Protocol::C_MOVE pkt);

    // 몬스터 생성 및 소멸
    void EnterMonster(MonsterRef monster);
    void LeaveMonster(uint64 objectId);

    // 입장 로직 세분화
    bool EnterRegister(PlayerSessionRef session, PlayerRef player); // 자료구조 등록만
    void SendEnterSpawns(PlayerSessionRef session, PlayerRef player); // 주변 정보 전송
    void EnterMapChange(PlayerSessionRef session, PlayerRef player); // 맵 이동 완료 처리

    // 유틸리티: 특정 위치에서 가장 가까운 플레이어 찾기 (몬스터 어그로용)
    PlayerRef FindNearestPlayer(Protocol::PositionInfo* pos, float range);
    GameMapRef GetMap() { return _map; }

    // 패킷 브로드캐스팅 (특정 Zone 혹은 방 전체)
    int32 BroadcastToZone(SendBufferRef sendBuffer, int32 zoneIndex, uint64 exceptId = 0);
    int32 Broadcast(SendBufferRef sendBuffer, uint64 exceptId = 0);

    // 전투 관련 로직 (스킬 사용, 피격 등)
    void HandleSkill(std::shared_ptr<Creature> attacker, int32 skillId);
    void OnMonsterMoved(MonsterRef monster);

    // 아이템 사용
    void HandleUseItem(PlayerSessionRef session, PlayerRef player, Protocol::C_USE_ITEM pkt);

    // 인벤토리 조작 (이동, 스왑, 합치기)
    void HandleInvDragDrop(PlayerSessionRef session, PlayerRef player, Protocol::C_INV_DRAG_DROP pkt);

    // 몬스터 사망 처리 (경험치 분배, 드랍, 디스폰 등)
    void HandleMonsterDead(std::shared_ptr<Creature> attacker, MonsterRef monster);

    // 채팅 브로드캐스팅
    void BroadcastChat(const Protocol::S_CHAT_NTF& ntf);

    // 강제 퇴장 처리 (세션 끊김 등)
    void LeaveById(PlayerSessionRef session, uint64 playerId);

    // =========================================================
    // [Handler Router - ById]
    // 네트워킹 레이어에서 호출되는 진입점들
    // ID로 객체를 찾아서 유효성 검사 후, 실제 로직(Handle...)을 호출함
    // =========================================================

    // ID로 플레이어 찾기 (GameRoom 스레드 내부에서만 호출 가능)
    PlayerRef FindPlayer_ActorOnly(uint64 playerId) const;

    // 클라이언트 패킷 -> JobQueue -> 여기서 처리
    void HandleMoveById(PlayerSessionRef session, uint64 playerId, Protocol::C_MOVE pkt);
    void HandleUseItemById(PlayerSessionRef session, uint64 playerId, Protocol::C_USE_ITEM pkt);
    void HandleEquipItemById(PlayerSessionRef session, uint64 playerId, Protocol::C_EQUIP_ITEM pkt);
    void HandleInvDragDropById(PlayerSessionRef session, uint64 playerId, Protocol::C_INV_DRAG_DROP pkt);
    void HandleRespawnById(PlayerSessionRef session, uint64 playerId);

    // [Trade System Handlers] 거래 관련 요청 처리
    void HandleTradeReqById(PlayerSessionRef session, uint64 fromPlayerId, uint64 targetPlayerId);
    void HandleTradeInviteRespById(PlayerSessionRef session, uint64 responderId, bool accept);
    void HandleTradeOfferSetById(PlayerSessionRef session, uint64 playerId, Protocol::C_TRADE_OFFER_SET pkt);
    void HandleTradeGoldSetById(PlayerSessionRef session, uint64 playerId, Protocol::C_TRADE_GOLD_SET pkt);
    void HandleTradeReadyById(PlayerSessionRef session, uint64 playerId, Protocol::C_TRADE_READY pkt);
    void HandleTradeConfirmById(PlayerSessionRef session, uint64 playerId, Protocol::C_TRADE_CONFIRM pkt);
    void HandleTradeCancelById(PlayerSessionRef session, uint64 playerId, Protocol::C_TRADE_CANCEL pkt);

    // 스킬 및 채팅 핸들러 라우터
    void HandleSkillById(PlayerSessionRef session, uint64 playerId, int32 skillId);
    void HandleChatById(PlayerSessionRef session, uint64 playerId, const std::string& msg);

    // 플레이어 부활 처리
    void HandleRespawn(PlayerSessionRef session, PlayerRef player);

    // 맵 이동 요청 처리 (DB 저장 후 다른 서버/채널로 이동)
    void TransferMapChangeById(PlayerSessionRef session,
        uint64 playerId,
        int32 targetChannelId,
        int32 targetMapId,
        int64 targetInstanceId,
        const Protocol::PositionInfo& spawn);

public:
    // =========================================================
    // [Projectile System]
    // 투사체(화살, 마법구 등) 관리
    // =========================================================
    void EnterProjectile(ProjectileRef p);
    void LeaveProjectile(uint64 projectileId);
    void OnProjectileMoved(ProjectileRef p);
    void UpdateProjectiles(uint64 deltaMs); // 매 프레임 위치 갱신 및 충돌 체크

    // 마을 귀환 위치 저장
    void SaveReturnLocation_ActorOnly(uint64 playerId);

    // 스킬 확장 버전 (방향 및 클라 시간 포함)
    void HandleSkill(std::shared_ptr<Creature> attacker, int32 skillId, float castYaw, uint32 clientTimeMs);
    void HandleSkillById(PlayerSessionRef session, uint64 playerId, int32 skillId, float castYaw, uint32 clientTimeMs);

    int32 GetChannelId() const { return _channelId; }
    int32 GetMapId() const { return _mapId; }

private:
    GameMapRef              _map;       // 네비메쉬 등 지형 정보
    std::shared_ptr<JobQueue> _jobQueue; // 이 방의 일감 큐

    // 빠른 검색을 위한 해시맵 (ID -> 객체)
    Map<uint64, PlayerRef>  _players;
    Map<uint64, MonsterRef> _monsters;

    SpatialGrid             _grid;      // AOI(시야 관리)를 위한 격자 시스템

    int32 _channelId = 1;
    int32 _mapId = 1;

    // =========================================================
    //  Instance Lifetime State
    // =========================================================
    int64 _instanceId = 0; // 0이면 월드맵, >0이면 인스턴스 던전 ID
    std::atomic<int32> _playerCount{ 0 };
    std::atomic<bool>  _closing{ false }; // 방이 닫히는 중인가?
    std::atomic<uint64> _emptySinceMs{ 0 }; // 언제부터 비어있었나 (Purge 체크용)

    std::unique_ptr<BattleSystem> _battle; // 전투 공식 계산 담당

private:
    // =========================================================
    // [AOI v2 - Area Of Interest]
    // 서버 성능 최적화의 핵심: 내 주변만 보여주고 통신한다.
    // =========================================================
    int32 _aoiNeighborRadiusCells = 2; // 내 주변 2칸(5x5)까지 관심 영역으로 설정
    float _interestRadius = 150.f;     // 실제 시야 거리 (미터)
    float _lazyUpdateDist = 10.f;      // 10m 이상 움직여야 갱신 (스로틀링)
    uint64 _lazyUpdateTickMs = 500;    // 혹은 0.5초가 지나야 갱신

    // 패킷 뭉쳐 보내기 설정 (6KB SendBufferChunk를 넘지 않도록 보수적으로 유지)
    int32 _batchSpawnPlayers = 20;
    int32 _batchSpawnMonsters = 40;
    int32 _batchDespawn = 200;

private:
    // AOI 메인 로직: 내가 누구를 봐야 하고, 누구를 안 보게 됐는지 계산
    void UpdateAOI(PlayerSessionRef session, PlayerRef me, bool forceFullSnapshot);
    bool ShouldUpdateAOI(PlayerRef me, bool zoneChanged) const;
    void SendMoveSync(PlayerSessionRef ownerSession,
        PlayerRef player,
        const Protocol::PositionInfo& posInfo,
        bool sendToOwner,
        bool sendToVisibleOthers);

    // 주변 Zone들을 뒤져서 플레이어/몬스터 후보군 수집
    void CollectCandidates(int32 zoneIndex, Vector<PlayerRef>& outPlayers, Vector<MonsterRef>& outMonsters);

    // 2D 거리 체크 (CPU 비용 아끼기 위해 제곱 거리 사용 가능)
    bool PassDistance2D(const Protocol::PositionInfo& a, const Protocol::PositionInfo& b, float r) const;

    // 네비메쉬 상에서 연결된 구역인지 확인 (벽 너머 투시 방지)
    uint32 GetConnectivityId_ActorOnly(const Protocol::PositionInfo& pos) const;

    // 패킷 전송 (배칭 처리)
    void SendSpawnBatchedToMe(PlayerSessionRef session,
        const Vector<PlayerRef>& spawnPlayers,
        const Vector<MonsterRef>& spawnMonsters,
        bool snapshotMode,
        uint32 snapshotId);

    void SendDespawnBatchedToMe(PlayerSessionRef session, const Vector<uint64>& objectIds);

    // 시야 반경을 그리드 셀 단위로 변환
    int32 EffectiveAoiRadiusCells() const;
    void BuildRoomWideVisibilityForPlayer_ActorOnly(PlayerRef player);
    void ClearRoomWideVisibilityForPlayer_ActorOnly(PlayerRef player);
    void SendRoomWideSnapshotToPlayer_ActorOnly(PlayerSessionRef session, PlayerRef player, bool snapshotMode);
    void BroadcastRoomWidePlayerSpawn_ActorOnly(PlayerRef player);

    // =========================================================
    // [Spawn System]
    // 고정 좌표 기반 몬스터 스폰/리스폰 관리
    // =========================================================
    struct SpawnPointRuntime
    {
        int32 spawnId = 0;
        int32 monsterId = 0;
        Protocol::PositionInfo pos;
        int32 maxAlive = 1;
        uint64 respawnMs = 0;
        int32 aliveCount = 0;
        uint64 nextSpawnMs = 0;
    };

    void InitSpawnPoints_ActorOnly();
    void UpdateSpawns_ActorOnly(uint64 nowMs);
    void SpawnFromPoint_ActorOnly(SpawnPointRuntime& sp, uint64 nowMs);
    void OnMonsterDespawned_ActorOnly(MonsterRef monster);


    // =========================================================
    // [Trade System v1]
    // 안전한 1:1 아이템 거래를 위한 상태 머신
    // 2-Phase Commit 유사 방식으로 구현하여 복사 버그 방지
    // =========================================================
    enum class TradeState : uint8
    {
        None = 0,
        Invited = 1,    // 초대 보냄
        Active = 2,     // 거래 창 열림 (아이템 올리는 중)
        Locked = 3,     // "준비 완료" (수정 불가)
        Committing = 4, // DB에 저장 요청 중 (모든 조작 불가)
    };

    // 거래 슬롯 하나 정보
    struct TradeOfferEntry
    {
        uint64 itemUid = 0;
        int32 templateId = 0;
        int32 count = 0;
    };

    // DB 트랜잭션을 위한 커밋 계획표
    struct TradeCommitPlan
    {
        // DB에 보낼 최종 스냅샷 (원자적 처리를 위함)
        Vector<Protocol::ItemInfo> finalAItems;
        Vector<uint64> deletedAItemUids;
        Vector<Protocol::ItemInfo> finalBItems;
        Vector<uint64> deletedBItemUids;

        int64 finalGoldA = 0;
        int64 finalGoldB = 0;

        // DB 성공 후 클라에게 알려줄 변경 사항
        Vector<Protocol::ItemInfo> notifyChangeA;
        Vector<uint64> notifyRemoveA;
        Vector<Protocol::ItemInfo> notifyChangeB;
        Vector<uint64> notifyRemoveB;
    };

    // 진행 중인 거래 세션 정보
    struct TradeSession
    {
        uint64 tradeId = 0;
        uint64 playerAId = 0;
        uint64 playerBId = 0;

        HashMap<uint64, TradeOfferEntry> offerA; // A가 올린 물건들
        HashMap<uint64, TradeOfferEntry> offerB; // B가 올린 물건들

        int64 offerGoldA = 0;
        int64 offerGoldB = 0;

        bool readyA = false;
        bool readyB = false;
        bool confirmA = false;
        bool confirmB = false;

        TradeState state = TradeState::None;

        uint64 createdAtMs = 0;
        uint64 lastTouchedMs = 0; // 타임아웃 체크용

        // Phase 2: DB 커밋 중일 때 필요한 정보 저장
        uint64 commitRequestId = 0;
        std::unique_ptr<TradeCommitPlan> commitPlan;
    };

    // 거래 로직 (Actor Thread에서만 실행됨 - Lock 불필요)
    void UpdateTrades_ActorOnly(uint64 nowMs);
    void CancelTrade_ActorOnly(uint64 tradeId, Protocol::TradeCancelReason reason, Protocol::TradeFailCode failCode = Protocol::TRADE_FAIL_NONE, const std::string& msg = "");
    void SendOfferUpdate_ActorOnly(uint64 tradeId, uint64 whoPlayerId);
    void SendReadyState_ActorOnly(uint64 tradeId);

    // 거래 성사 시 DB 커밋 플랜 작성
    bool BuildTradeCommitPlan_ActorOnly(uint64 tradeId, TradeCommitPlan& outPlan, Protocol::TradeFailCode& outFail, std::string& outMsg);
    bool StartTradeCommitPhase2_ActorOnly(uint64 tradeId, Protocol::TradeFailCode& outFail, std::string& outMsg);
    void OnTradeCommitResult_ActorOnly(const Protocol::S2S_RES_TRADE_COMMIT& pkt);

    // 헬퍼 함수
    TradeSession* FindTrade_ActorOnly(uint64 tradeId);
    TradeSession* FindTradeByPlayer_ActorOnly(uint64 playerId);

    static constexpr int32 kTradeMaxInventorySlots = 24;
    static constexpr uint64 kTradeTimeoutMs = 60'000; // 1분 동안 반응 없으면 취소

    HashMap<uint64, TradeSession> _trades;      // 진행 중인 모든 거래
    HashMap<uint64, uint64> _tradeByPlayer;     // 플레이어 ID -> 거래 ID 매핑

    Map<uint64, ProjectileRef> _projectiles;    // 방에 날아다니는 투사체들
    uint64 _lastUpdateMs = 0; // 델타 타임 계산용

    Map<int32, SpawnPointRuntime> _spawnPoints;
};

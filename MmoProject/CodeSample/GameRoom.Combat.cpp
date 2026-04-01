#include "pch.h"
#include "GameRoom.h"
#include "GameMap.h"
#include "Player.h"
#include "ExperimentUtils.h"
#include "RoomManager.h"
#include "GameRoom.Net.h"
#include "Projectile.h"
#include "GameMetrics.h"


void GameRoom::HandleSkill(std::shared_ptr<Creature> attacker, int32 skillId, float castYaw, uint32 clientTimeMs)
{
    if (attacker == nullptr)
        return;

    (void)clientTimeMs; // 나중에 쿨타임 검증용으로 쓸 예정

    // 방 검증: 공격자가 현재 이 방에 있는지 확인 (비정상 패킷 방어)
    if (attacker->GetRoom().get() != this)
        return;

    // 데이터 매니저에서 스킬 정보 조회
    const Protocol::SkillTemplateInfo* skillData = DataManager::Instance()->GetSkillTemplate(skillId);
    if (skillData == nullptr)
        return;

    // 사망 상태면 스킬 사용 불가
    if (auto* st = attacker->GetStatInfo())
    {
        if (st->hp() <= 0)
            return;
    }

    if (attacker->GetPosInfo() && attacker->GetPosInfo()->actionstate() == Protocol::ACTION_DEAD)
        return;

    // 서버 쿨타임 검증 (권위)
    if (attacker->CanUseSkill(skillId) == false)
        return;

    const Protocol::SkillType type = skillData->skilltype();

    // =========================================================
    //  [PROJECTILE 스킬 처리]
    //  즉시 데미지를 입히지 않고, 투사체 오브젝트를 생성해서 날려보냄
    // =========================================================
    if (type == Protocol::SKILL_PROJECTILE)
    {
        // 쿨타임 소비
        attacker->StartSkillCooldown(skillId, skillData->cooldown());

        // 1. 스킬 사용 모션부터 주변에 브로드캐스트 (선딜레이 표현)
        const int32 zoneIndex = _grid.GetZoneIndex(*attacker->GetPosInfo());
        {
            Protocol::S_SKILL skillPkt;
            skillPkt.set_objectid(NetId(attacker));
            skillPkt.set_skillid(skillId);
            skillPkt.set_cooldownms(skillData->cooldown());

            SendBufferRef skillBuffer = ClientPacketHandler::MakeSendBuffer(skillPkt);
            const bool roomWideBaseline = ExperimentUtils::IsHotRoomRoomWideBaseline();
            const uint64 exceptId = (roomWideBaseline && attacker->GetObjectType() == Protocol::OBJECT_TYPE_PLAYER)
                ? std::static_pointer_cast<Player>(attacker)->GetPlayerId()
                : 0;
            const int32 recipients = roomWideBaseline
                ? Broadcast(skillBuffer, exceptId)
                : BroadcastToZone(skillBuffer, zoneIndex);
            GameMetrics::OnBroadcastRecipients(
                GameMetrics::HotRoomBroadcastKind::Skill,
                roomWideBaseline ? GameMetrics::HotRoomBroadcastMode::Room : GameMetrics::HotRoomBroadcastMode::Aoi,
                static_cast<std::size_t>(recipients));
        }

        // 2. 투사체 속성 설정 (속도, 수명, 히트 반경 등)
        float speed = skillData->projectilespeed();
        uint32 lifeMs = (uint32)skillData->projectilelifems();

        float hitRadius = skillData->hitradius();
        if (hitRadius <= 0.0f)
            hitRadius = skillData->radius(); // 데이터 없으면 기본 반경 사용

        bool stopOnHit = skillData->stoponhit(); // 관통 여부
        int32 maxHits = skillData->maxhits();
        if (maxHits <= 0) maxHits = 1;

        // 사거리 계산: 데이터에 없으면 속도 * 시간으로 계산
        float range = skillData->range();
        if (range <= 0.0f && lifeMs > 0 && speed > 0.0f)
            range = speed * ((float)lifeMs / 1000.0f);

        // 안전장치: 데이터 미스나 해킹으로 인한 무한 투사체 방지
        if (lifeMs == 0 && range <= 0.0f)
            lifeMs = 800; // 기본 0.8초

        // 3. 투사체 시작 위치 및 방향 설정
        Protocol::PositionInfo startPos = *attacker->GetPosInfo();

        // 플레이어는 클라가 바라보는 방향(castYaw)을 믿어줌 (조작감 때문)
        if (attacker->GetObjectType() == Protocol::OBJECT_TYPE_PLAYER)
            startPos.set_yaw(castYaw);

        // 몬스터는 서버 AI가 정한 방향이 권위(Authority)를 가짐

        // 4. 소유자 ID 설정 (킬 판정이나 경험치 분배용)
        uint64 ownerId = 0;
        if (attacker->GetObjectType() == Protocol::OBJECT_TYPE_PLAYER)
            ownerId = std::static_pointer_cast<Player>(attacker)->GetPlayerId();
        else
            ownerId = attacker->GetObjectId();

        // 5. 투사체 생성 및 방에 입장 시키기
        // 빈번하게 생성/삭제되므로 ObjectPool 사용
        ProjectileRef p = ObjectPool<Projectile>::MakeShared();
        p->Init(ownerId, skillId, startPos, speed, lifeMs, range);
        p->SetCombatParams(hitRadius, stopOnHit, maxHits);

        EnterProjectile(p);
        return;
    }

    // =========================================================
    //  [INSTANT / AUTO 스킬 처리]
    //  투사체가 아닌 즉발형 공격 (평타, 타겟팅 스킬 등)
    // =========================================================
    if (_battle == nullptr)
        return;

    SkillResult result;
    // BattleManager에게 판정 위임 (명중, 크리티컬, 데미지 계산)
    if (_battle->ResolveSkill(attacker, skillId, castYaw, result) == false)
        return;

    // 쿨타임 소비 (명중 판정 성공 시)
    attacker->StartSkillCooldown(skillId, skillData->cooldown());

    // 1. 스킬 모션 패킷 전송
    {
        Protocol::S_SKILL skillPkt;
        skillPkt.set_objectid(NetId(attacker));
        skillPkt.set_skillid(skillId);
        skillPkt.set_cooldownms(skillData->cooldown());

        SendBufferRef skillBuffer = ClientPacketHandler::MakeSendBuffer(skillPkt);
        const bool roomWideBaseline = ExperimentUtils::IsHotRoomRoomWideBaseline();
        const uint64 exceptId = (roomWideBaseline && attacker->GetObjectType() == Protocol::OBJECT_TYPE_PLAYER)
            ? std::static_pointer_cast<Player>(attacker)->GetPlayerId()
            : 0;
        const int32 recipients = roomWideBaseline
            ? Broadcast(skillBuffer, exceptId)
            : BroadcastToZone(skillBuffer, result.zoneIndex);
        GameMetrics::OnBroadcastRecipients(
            GameMetrics::HotRoomBroadcastKind::Skill,
            roomWideBaseline ? GameMetrics::HotRoomBroadcastMode::Room : GameMetrics::HotRoomBroadcastMode::Aoi,
            static_cast<std::size_t>(recipients));
    }

    // 2. 피격 결과(HP 감소) 패킷 전송
    for (const HitInfo& hit : result.hits)
    {
        auto victim = hit.target;
        if (victim == nullptr) continue;

        Protocol::S_CHANGE_HP changePkt;
        changePkt.set_objectid(NetId(victim));
        changePkt.set_attackerid(NetId(attacker));
        changePkt.set_currenthp(victim->GetStatInfo()->hp());
        changePkt.set_damage(hit.damage);

        SendBufferRef sendBuffer = ClientPacketHandler::MakeSendBuffer(changePkt);
        const bool roomWideBaseline = ExperimentUtils::IsHotRoomRoomWideBaseline();
        const int32 recipients = roomWideBaseline
            ? Broadcast(sendBuffer)
            : BroadcastToZone(sendBuffer, result.zoneIndex);
        GameMetrics::OnBroadcastRecipients(
            GameMetrics::HotRoomBroadcastKind::Hp,
            roomWideBaseline ? GameMetrics::HotRoomBroadcastMode::Room : GameMetrics::HotRoomBroadcastMode::Aoi,
            static_cast<std::size_t>(recipients));
    }
}

// 하위 호환성을 위한 오버로딩 (기존 코드 깨짐 방지)
void GameRoom::HandleSkill(std::shared_ptr<Creature> attacker, int32 skillId)
{
    HandleSkill(attacker, skillId, /*castYaw=*/0.f, /*clientTimeMs=*/0);
}


// 패킷 핸들러에서 호출하는 진입점
void GameRoom::HandleSkillById(PlayerSessionRef session, uint64 playerId, int32 skillId, float castYaw, uint32 clientTimeMs)
{
    auto it = _players.find(playerId);
    if (it == _players.end())
        return;

    PlayerRef player = it->second;
    if (!player) return;

    // Creature 타입으로 캐스팅해서 공용 로직 태움
    std::shared_ptr<Creature> attacker = std::static_pointer_cast<Creature>(player);
    HandleSkill(attacker, skillId, castYaw, clientTimeMs);
}

// 하위 호환성 유지
void GameRoom::HandleSkillById(PlayerSessionRef session, uint64 playerId, int32 skillId)
{
    HandleSkillById(session, playerId, skillId, /*castYaw=*/0.f, /*clientTimeMs=*/0);
}

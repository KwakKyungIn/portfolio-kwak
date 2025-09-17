using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public enum ResourceNodeType
{
    Undefined, // 정의되지 않음
    Tree,      // 나무
    Ore        // 광석
}

[RequireComponent(typeof(BoxCollider2D))] // Collider 필수
public class ResourceNode : UsingTool // UsingTool에서 상속
{
    [SerializeField] GameObject pickUpDrop; // (사용 안함, 예전 드랍용)
    [SerializeField] Item item;             // 드랍할 아이템
    [SerializeField] int itemCountInOneDrop = 1; // 드랍당 아이템 개수
    [SerializeField] float spread = 0.7f;   // 드랍 산포 범위
    [SerializeField] int dropCount = 5;     // 총 드랍 횟수
    [SerializeField] ResourceNodeType nodeType; // 노드 종류 (나무/광석 등)

    public override void Hit()
    {
        // 드랍 횟수만큼 반복
        while (dropCount > 0)
        {
            dropCount -= 1;

            Vector3 position = transform.position; // 기본 위치
            position.x += spread * UnityEngine.Random.value - spread / 2; // x 랜덤 산포
            position.y += spread * UnityEngine.Random.value - spread / 2; // y 랜덤 산포

            // 아이템 스폰 매니저를 통해 드랍
            ItemSpawnManager.instance.SpawnItem(position, item, itemCountInOneDrop);
        }

        Destroy(gameObject); // 노드 파괴 (나무 잘려나감)
    }

    public override bool CanBeHit(List<ResourceNodeType> canBeHit)
    {
        return canBeHit.Contains(nodeType); // 허용된 타입이면 채집 가능
    }
}

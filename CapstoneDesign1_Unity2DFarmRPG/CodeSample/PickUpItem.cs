using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class PickUpItem : MonoBehaviour
{
    Transform player;                   // 플레이어 위치 참조
    [SerializeField] float speed = 5f;  // 흡수 속도
    [SerializeField] float PickUpDistance = 1.5f; // 흡수 거리
    [SerializeField] float ttl = 10f;   // 아이템 생존 시간 (초)

    public Item item;   // 아이템 종류
    public int count = 1; // 아이템 개수

    private void Awake()
    {
        player = GameManager.instance.player.transform; // 플레이어 위치 가져오기
    }

    public void Set(Item item, int count)
    {
        this.item = item;   // 아이템 설정
        this.count = count; // 개수 설정

        SpriteRenderer renderer = GetComponent<SpriteRenderer>(); // 스프라이트 컴포넌트 가져오기
        renderer.sprite = item.icon; // 아이콘으로 표시
    }

    private void Update()
    {
        ttl -= Time.deltaTime;          // 시간 차감
        if (ttl < 0) Destroy(gameObject); // 수명 끝나면 파괴

        float distance = Vector3.Distance(transform.position, player.position); // 플레이어와 거리 계산
        if (distance > PickUpDistance) return; // 흡수 거리 밖이면 리턴

        // 플레이어 쪽으로 이동
        transform.position = Vector3.MoveTowards(
            transform.position,
            player.position,
            speed * Time.deltaTime
        );

        if (distance < 0.1f) // 충분히 가까우면
        {
            if (GameManager.instance.inventoryContainer != null)
            {
                // 인벤토리에 추가
                GameManager.instance.inventoryContainer.Add(item, count);
            }
            else
            {
                Debug.LogWarning("인벤토리가 존재하지 않음"); // 예외 상황 처리
            }
            Destroy(gameObject); // 아이템 파괴
        }
    }
}

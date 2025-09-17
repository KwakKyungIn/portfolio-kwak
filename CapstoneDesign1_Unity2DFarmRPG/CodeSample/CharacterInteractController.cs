using System.Collections;  
using System.Collections.Generic;  
using UnityEngine;  
using UnityEngine.TextCore.Text;  

public class CharacterInteractController : MonoBehaviour
{
    CharacterController2D characterController;    // 캐릭터 이동/방향 정보 가져오기
    Rigidbody2D rgbd2d;                          // 캐릭터의 위치 확인용
    [SerializeField] float offsetDistance = 1f;  // 캐릭터 앞 위치 오프셋
    [SerializeField] float sizeOfInteractableArea = 1.2f; // 탐지 범위 크기
    Character character;                         // 캐릭터 자체 참조
    [SerializeReference] HighlightController highlightController; // 상호작용 대상 하이라이트

    private void Awake()
    {
        characterController = GetComponent<CharacterController2D>(); // 캐릭터 이동 컨트롤러 가져오기
        rgbd2d = GetComponent<Rigidbody2D>();                        // Rigidbody2D 가져오기
        character = GetComponent<Character>();                       // Character 스크립트 가져오기
    }

    private void Update()
    {
        Checked(); // 항상 앞에 있는 상호작용 가능한 대상 체크

        if (Input.GetMouseButtonDown(1)) // 마우스 오른쪽 버튼 클릭 시
        {
            Interact(); // 실제 상호작용 실행
        }
    }

    private void Checked()
    {
        // 캐릭터 위치 + 마지막 이동 방향 * 거리 → 상호작용 탐지 위치
        Vector2 position = rgbd2d.position + characterController.lastMotionVector * offsetDistance;

        // 탐지 위치 주위에 있는 모든 Collider 검색
        Collider2D[] colliders = Physics2D.OverlapCircleAll(position, sizeOfInteractableArea);

        // 검색된 Collider 중에서 Interactable을 찾음
        foreach (Collider2D c in colliders)
        {
            Interactable hit = c.GetComponent<Interactable>();
            if (hit != null)
            {
                highlightController.Highlight(hit.gameObject); // 하이라이트 표시
                return; // 하나만 찾으면 종료
            }
        }
        highlightController.Hide(); // 없으면 하이라이트 숨기기
    }

    private void Interact()
    {
        // 캐릭터 앞쪽 위치 계산
        Vector2 position = rgbd2d.position + characterController.lastMotionVector * offsetDistance;

        // 탐지 영역 내 Collider 검색
        Collider2D[] colliders = Physics2D.OverlapCircleAll(position, sizeOfInteractableArea);

        // Interactable이 있으면 상호작용 실행
        foreach (Collider2D c in colliders)
        {
            Interactable hit = c.GetComponent<Interactable>();
            if (hit != null)
            {
                hit.Interact(character); // 캐릭터를 인자로 전달
                break; // 첫 번째 대상만 상호작용
            }
        }
    }
}

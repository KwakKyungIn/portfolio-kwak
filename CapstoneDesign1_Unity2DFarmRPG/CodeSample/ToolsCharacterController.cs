using System.Collections;
using System.Collections.Generic;
using System.Xml.Serialization;
using UnityEngine;
using UnityEngine.Tilemaps;

public class ToolsCharacterController : MonoBehaviour
{
    CharacterController2D character;                 // 캐릭터 이동 컨트롤러
    Rigidbody2D rgbd2d;                              // 캐릭터 위치/물리 제어
    ToolBarController toolbarController;             // 툴바에서 아이템 가져오기
    [SerializeField] float offsetDistance = 1f;      // 툴 사용 위치 오프셋
    [SerializeField] float sizeOfInteractableArea = 1.2f; // 상호작용 범위
    [SerializeField] MarkerManager markerManager;    // 선택 타일 하이라이트
    [SerializeField] float maxDistance = 1.5f;       // 타일 선택 가능 최대 거리
    [SerializeField] TileMapReadController tileMapReadcontroller; // 마우스 좌표 → 타일 좌표 변환
    [SerializeField] ToolAction onTilePickUp;        // 빈손일 때 타일 픽업 액션

    Vector3Int selectedTilePosition; // 현재 선택된 타일 좌표
    bool selectable;                 // 선택 가능한지 여부

    private void Awake()
    {
        character = GetComponent<CharacterController2D>(); // 캐릭터 제어 컴포넌트 가져오기
        rgbd2d = GetComponent<Rigidbody2D>();              // Rigidbody2D 가져오기
        toolbarController = GetComponent<ToolBarController>(); // 툴바 컨트롤러 가져오기
    }

    private void Update()
    {
        SelectTile();       // 마우스 위치 기준 타일 선택
        CanSelectCheck();   // 선택 가능한지 확인
        Marker();           // 하이라이트 표시

        if (Input.GetMouseButtonDown(0)) // 좌클릭 입력
        {
            if (UseToolWorld() == true)  // 월드 오브젝트에 툴 사용
            {
                return;                  // 성공했으면 종료
            }
            UseToolGrid();               // 실패 시 타일에 툴 사용
        }
    }

    private void SelectTile()
    {
        // 마우스 화면 좌표 → 타일맵 좌표로 변환
        selectedTilePosition = tileMapReadcontroller.GetGridPosition(Input.mousePosition, true);
    }

    void CanSelectCheck()
    {
        Vector2 characterPosition = transform.position; // 캐릭터 위치
        Vector2 cameraPosition = Camera.main.ScreenToWorldPoint(Input.mousePosition); // 마우스 월드 좌표
        // 캐릭터와 마우스 거리 비교 → 일정 거리 이하면 선택 가능
        selectable = Vector2.Distance(characterPosition, cameraPosition) < maxDistance;
        markerManager.Show(selectable); // 선택 가능 여부를 하이라이트로 표시
    }

    private void Marker()
    {
        // 현재 선택된 타일 좌표를 마커에 전달
        markerManager.markedCellPosition = selectedTilePosition;
    }

    private bool UseToolWorld()
    {
        // 캐릭터 앞쪽 위치 계산
        Vector2 position = rgbd2d.position + character.lastMotionVector * offsetDistance;

        Item item = toolbarController.GetItem; // 툴바에서 현재 아이템 가져오기
        if (item == null) return false;        // 아이템 없으면 실패
        if (item.onAction == null) return false; // 월드 액션 없으면 실패

        // 월드 액션 실행
        bool complete = item.onAction.OnApply(position);

        if (complete == true)
        {
            if (item.onItemUsed != null) // 소모/내구도 처리
            {
                item.onItemUsed.OnItemUsed(item, GameManager.instance.inventoryContainer);
            }
        }

        return complete; // 성공 여부 반환
    }

    private void UseToolGrid()
    {
        if (selectable == true) // 선택 가능한 타일만 처리
        {
            Item item = toolbarController.GetItem; // 툴바 아이템 가져오기

            if (item == null) // 아이템 없으면 타일 픽업 시도
            {
                PickUpTile();
                return;
            }
            if (item.onTileMapAction == null) return; // 타일 액션 없으면 종료

            // 타일 액션 실행
            bool complete = item.onTileMapAction.OnApplyToTileMap(
                selectedTilePosition,
                tileMapReadcontroller,
                item);

            if (complete == true) // 성공 시 아이템 사용 처리
            {
                if (item.onItemUsed != null)
                {
                    item.onItemUsed.OnItemUsed(item, GameManager.instance.inventoryContainer);
                }
            }
        }
    }

    private void PickUpTile()
    {
        if (onTilePickUp == null) return; // 픽업 액션 없으면 종료

        // 타일 픽업 액션 실행
        onTilePickUp.OnApplyToTileMap(selectedTilePosition, tileMapReadcontroller, null);
    }
}

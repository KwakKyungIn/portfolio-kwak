using System.Collections;
using System.Collections.Generic;
using System.Xml.Serialization;
using UnityEngine;
using UnityEngine.Tilemaps;

public class CropTile
{
    public int growTimer;             // 경과 시간(틱 단위)
    public int growStage;             // 현재 성장 단계
    public Crop crop;                 // 연결된 작물 데이터
    public SpriteRenderer renderer;   // 스프라이트 표시용
}

public class CropsManager : TimeAgent
{
    [SerializeField] TileBase plowed;               // 밭갈기 타일
    [SerializeField] TileBase seeded;               // 씨앗 심은 타일
    [SerializeField] Tilemap targetTilemap_Crop;    // 작물 표시용 타일맵
    [SerializeField] Tilemap targetTilemap_Plow;    // 밭갈기 표시용 타일맵
    [SerializeField] GameObject cropsSpritePrefab;  // 작물 스프라이트 프리팹

    Dictionary<Vector2Int, CropTile> crops;         // 위치별 작물 관리 딕셔너리

    private void Start()
    {
        crops = new Dictionary<Vector2Int, CropTile>(); // 딕셔너리 초기화
        onTimeTick += Tick;                             // 시간 이벤트 구독
        Init();                                         // TimeAgent 초기화
    }

    private void Tick()
    {
        // 모든 작물 타일 순회
        foreach (CropTile cropTile in crops.Values)
        {
            if (cropTile.crop == null) continue;     // 작물이 없으면 건너뜀

            cropTile.growTimer += 1;                 // 성장 시간 1 증가

            // 현재 단계 도달 시 스프라이트 갱신
            if (cropTile.growTimer >= cropTile.crop.growthStageTime[cropTile.growStage])
            {
                cropTile.renderer.gameObject.SetActive(true);
                cropTile.renderer.sprite = cropTile.crop.sprites[cropTile.growStage];
                cropTile.growStage += 1;             // 다음 성장 단계로 이동
            }

            // 최종 성장 시간 도달 시 crop 제거
            if (cropTile.growTimer >= cropTile.crop.timeToGrow)
            {
                Debug.Log("자랄 준비 끝.");         // 성장 완료 로그
                cropTile.crop = null;                // 수확 준비
            }
        }
    }

    public bool Check(Vector3Int position)
    {
        return crops.ContainsKey((Vector2Int)position); // 해당 위치에 작물이 있는지 확인
    }

    public void Plow(Vector3Int position)
    {
        if (crops.ContainsKey((Vector2Int)position)) return; // 이미 있으면 무시
        CreatePlowedTile(position);                          // 새 밭갈기 타일 생성
    }

    public void Seed(Vector3Int position, Crop toSeed)
    {
        targetTilemap_Crop.SetTile(position, seeded);        // 씨앗 심은 타일 표시
        crops[(Vector2Int)position].crop = toSeed;           // 딕셔너리에 작물 데이터 연결
    }

    private void CreatePlowedTile(Vector3Int position)
    {
        CropTile crop = new CropTile();                      // 새로운 CropTile 생성
        crops.Add((Vector2Int)position, crop);               // 딕셔너리에 추가

        GameObject go = Instantiate(cropsSpritePrefab);      // 스프라이트 프리팹 생성
        go.transform.position = targetTilemap_Crop.CellToWorld(position); // 타일 위치에 배치
        go.SetActive(false);                                 // 기본은 숨김 상태
        crop.renderer = go.GetComponent<SpriteRenderer>();   // 렌더러 저장

        targetTilemap_Plow.SetTile((Vector3Int)position, plowed); // 밭갈기 타일맵 갱신
    }
}

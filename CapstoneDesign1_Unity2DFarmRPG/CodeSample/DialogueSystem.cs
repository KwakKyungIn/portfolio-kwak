using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

public class DialogueSystem : MonoBehaviour
{
    [SerializeField] Text targetText;       // 대사 텍스트 출력
    [SerializeField] Text nameText;         // 배우 이름 출력
    [SerializeField] Image portrait;        // 배우 초상화
    DialogueContainer currentDialogue;      // 현재 진행 중인 대화 데이터
    int currentTextLine;                    // 현재 줄 인덱스
    [Range(0f, 1f)]
    [SerializeField] float visibleTextPercent; // 현재 출력된 비율
    [SerializeField] float timePerLetter = 0.05f; // 글자당 출력 시간
    float totalTimeToType, currentTime;     // 타이핑 제어 변수
    string lineToShow;                      // 현재 보여줄 줄

    private void Update()
    {
        if (Input.GetMouseButtonDown(0))    // 마우스 좌클릭 시
        {
            PushText();                     // 텍스트 스킵/다음 줄 진행
        }
        TypeOutText();                      // 타이핑 효과 실행
    }

    private void TypeOutText()
    {
        if (visibleTextPercent >= 1f) return;         // 이미 다 출력했으면 종료
        currentTime += Time.deltaTime;                // 시간 누적
        visibleTextPercent = currentTime / totalTimeToType; // 현재 출력 비율 계산
        visibleTextPercent = Mathf.Clamp(visibleTextPercent, 0, 1f);
        UpdateText();                                 // 텍스트 갱신
    }

    void UpdateText()
    {
        int letterCount = (int)(lineToShow.Length * visibleTextPercent); // 출력할 글자 수 계산
        targetText.text = lineToShow.Substring(0, letterCount);          // 부분 문자열 출력
    }

    private void PushText()
    {
        if (visibleTextPercent < 1f)
        {
            visibleTextPercent = 1f;   // 남은 텍스트 한 번에 출력
            UpdateText();
            return;
        }

        if (currentTextLine >= currentDialogue.line.Count)
        {
            Conclude();               // 대화 종료
        }
        else
        {
            CycleLine();              // 다음 줄로 넘어감
        }
    }

    void CycleLine()
    {
        lineToShow = currentDialogue.line[currentTextLine]; // 현재 줄 가져오기
        totalTimeToType = lineToShow.Length * timePerLetter; // 전체 타이핑 시간 계산
        currentTime = 0f;                                    // 시간 초기화
        visibleTextPercent = 0f;                             // 비율 초기화
        targetText.text = "";                                // 출력 비우기
        currentTextLine += 1;                                // 다음 줄 인덱스로 이동
    }

    public void Initialize(DialogueContainer dialogueContainer)
    {
        Show(true);                          // 대화창 활성화
        currentDialogue = dialogueContainer; // 대화 데이터 저장
        currentTextLine = 0;                 // 첫 줄부터 시작
        CycleLine();                         // 첫 줄 출력 준비
        UpdatePortrait();                    // 배우 정보 갱신
    }

    private void UpdatePortrait()
    {
        portrait.sprite = currentDialogue.actor.portrait; // 초상화 표시
        nameText.text = currentDialogue.actor.Name;       // 배우 이름 표시
    }

    private void Show(bool v)
    {
        gameObject.SetActive(v);             // 오브젝트 활성/비활성
    }

    private void Conclude()
    {
        Debug.Log("the dialogue has ended"); // 디버그 로그 출력
        Show(false);                         // 대화창 비활성화
    }
}

#include "pch.h"
#include "DeadLockProfiler.h"

// =============================================
// DeadLockProfiler
// - 스레드별로 "획득한 락 스택(LLockStack)"을 추적하고
// - 전역적으로 "락 간 선후 관계 그래프(_lockHistory)"를 관리
// - 새 에지(prev -> curr) 생길 때마다 DFS로 사이클 검사
//   → 순환 발생 시 현재 경로를 출력하고 CRASH()
// =============================================

/* PushLock(name)
   - [이 스레드]에서 name 락을 획득했다고 알림
   - 전역 맵에 이름→ID 부여(신규면 등록), 직전 락이 있으면 (prev->curr) 에지 추가
   - 처음 보는 에지면 바로 CheckCycle() 호출하여 순환 여부 확인
   - 마지막으로 스레드 로컬 스택(LLockStack)에 curr를 푸시
   - 주의: 전역 상태 보호용 _lock을 걸어 맵/그래프 변경을 직렬화 */
void DeadLockProfiler::PushLock(const char* name)
{
	LockGuard guard(_lock); // 전역 자료구조 보호

	int32 lockId = 0;

	// 이름 → ID 매핑 (처음 보는 이름이면 새 ID 부여)
	auto findIt = _nameToId.find(name);
	if (findIt == _nameToId.end())
	{
		lockId = static_cast<int32>(_nameToId.size());
		_nameToId[name] = lockId;
		_idToName[lockId] = name; // 역매핑도 보관 (에러 출력용)
	}
	else
	{
		lockId = findIt->second;
	}

	// (직전 락 → 현재 락) 순서 관계 기록
	if (LLockStack.empty() == false)
	{
		const int32 prevId = LLockStack.top();
		if (lockId != prevId) // 같은 락을 중복 푸시하는 케이스는 스킵
		{
			set<int32>& history = _lockHistory[prevId]; // prev에서 뻗어 나가는 도착 노드 집합
			if (history.find(lockId) == history.end())
			{
				history.insert(lockId); // 새로운 에지(prev -> curr) 확정
				CheckCycle();           // 새 에지 추가 시 한 번 즉시 검증
			}
		}
	}

	// 스레드 로컬 락 스택에 현재 락 푸시
	LLockStack.push(lockId);
}

/* PopLock(name)
   - [이 스레드]에서 name 락을 해제했다고 알림
   - 스택이 비었거나, top과 name이 불일치면 프로토콜 위반 → CRASH
   - 정상이라면 top pop */
void DeadLockProfiler::PopLock(const char* name)
{
	LockGuard guard(_lock); // 전역 상태 접근 보호(스택도 함께 보호)

	if (LLockStack.empty())
		CRASH("MULTIPLE_UNLOCK"); // 잠금 없이 해제 시도

	int32 lockId = _nameToId[name];
	if (LLockStack.top() != lockId)
		CRASH("INVALID_UNLOCK");  // 획득/해제 순서 불일치

	LLockStack.pop();
}

/* CheckCycle()
   - 현재까지의 락 순서 그래프에 대해 전수 DFS 수행
   - N개의 락 ID에 대해 discovered/finished/parent 배열 초기화 후
	 모든 정점에서 Dfs() 시도 (미발견 정점만 실제 진입)
   - 사이클 발견 시 Dfs() 내부에서 경로를 출력하고 CRASH */
void DeadLockProfiler::CheckCycle()
{
	const int32 lockCount = static_cast<int32>(_nameToId.size());

	// DFS bookkeeping 초기화
	_discoveredOrder = vector<int32>(lockCount, -1); // 미발견 = -1
	_discoveredCount = 0;
	_finished = vector<bool>(lockCount, false);
	_parent = vector<int32>(lockCount, -1);

	// 모든 정점에 대해 DFS 시도
	for (int32 lockId = 0; lockId < lockCount; lockId++)
		Dfs(lockId);

	// 사용 끝났으면 메모리 정리(용량 해제까지 원치 않으면 clear 유지)
	_discoveredOrder.clear();
	_finished.clear();
	_parent.clear();
}

/* Dfs(here)
   - 표준 DFS로 백에지(back edge) 탐지
   - 규칙:
	 * 처음 방문 시 discoveredOrder[here] 설정
	 * 이웃 there 순회:
	   - 미발견이면 parent[there]=here 기록 후 재귀
	   - 이미 발견됐고 yet-finished라면(스택 위에 있다면) → back edge
		 → 사이클 경로를 출력하고 CRASH
	 * 모든 이웃 처리 후 finished[here]=true */
void DeadLockProfiler::Dfs(int32 here)
{
	if (_discoveredOrder[here] != -1) // 이미 방문한 노드
		return;

	_discoveredOrder[here] = _discoveredCount++; // 방문 순서 기록

	// here에서 나가는 에지가 없으면 즉시 종료
	auto findIt = _lockHistory.find(here);
	if (findIt == _lockHistory.end())
	{
		_finished[here] = true;
		return;
	}

	// 인접 정점 순회
	set<int32>& nextSet = findIt->second;
	for (int32 there : nextSet)
	{
		if (_discoveredOrder[there] == -1)
		{
			// 트리 에지 : 처음 방문
			_parent[there] = here;
			Dfs(there);
			continue;
		}

		// 이미 발견된 정점이면, here가 더 먼저(작은 번호) 발견됐다면 전방/교차 에지 → 패스
		if (_discoveredOrder[here] < _discoveredOrder[there])
			continue;

		// 아직 finish되지 않은 정점으로의 에지라면 = 백에지 → 사이클!
		if (_finished[there] == false)
		{
			// 사이클 헤드(there)부터 here까지 경로 출력
			printf("%s -> %s\n", _idToName[here], _idToName[there]);

			int32 now = here;
			while (true)
			{
				printf("%s -> %s\n", _idToName[_parent[now]], _idToName[now]);
				now = _parent[now];
				if (now == there)
					break;
			}

			CRASH("DEADLOCK_DETECTED"); // 즉시 크래시로 알림 (개발 단계에서 빠르게 캐치)
		}
	}

	_finished[here] = true; // 후처리 완료
}

#pragma once
#include <vector>
#include <map>
#include <stack>

// 주의: 구현부에서 unordered_map / set 사용.
// (여기서는 원본 코드 유지: include는 건드리지 않음)

/*-----------------------------------------
 * DeadLockProfiler
 * - 락 획득/해제 시점을 알려주면
 *   "락 순서 그래프"를 갱신하고 즉시 사이클 검사
 * - 사용 패턴(예):
 *     PushLock("A");  // A 락 획득 직후
 *     PushLock("B");  // A 잡은 상태에서 B까지 획득 → A->B 에지 기록
 *     PopLock("B");   // B 해제
 *     PopLock("A");   // A 해제
 * - 에지(prev->curr)가 새로 생길 때마다 DFS로 순환 탐지
 *   → 개발 중 조기 데드락 원인 파악에 유용
 ------------------------------------------*/
class DeadLockProfiler
{
public:
	// 락 획득 알림: 내부 그래프(prev->curr) 갱신 및 사이클 검사
	void PushLock(const char* name);

	// 락 해제 알림: 현재 스레드의 획득 스택과 일치하는지 검증
	void PopLock(const char* name);

	// 전체 그래프에 대해 DFS로 사이클 검사(강제 호출용)
	void CheckCycle();

private:
	// DFS 한 노드 진입 (백에지 탐지 시 CRASH)
	void Dfs(int32 index);

private:
	// 이름 → ID (전역 고유 번호 매김)
	unordered_map<const char*, int32> _nameToId;

	// ID → 이름 (사이클 출력용)
	unordered_map<int32, const char*> _idToName;

	// 선후관계 그래프: fromID → { toID... }
	map<int32, set<int32>> _lockHistory;

	// 전역 보호용 뮤텍스 (그래프/맵/스택을 직렬화)
	Mutex _lock;

private:
	// DFS 상태 배열들
	vector<int32>  _discoveredOrder;   // 최초 방문 순서(-1=미방문)
	int32          _discoveredCount = 0;
	vector<bool>   _finished;          // 후처리 완료 여부
	vector<int32>  _parent;            // DFS 트리의 부모

	// 스레드별 락 스택(LLockStack)은 외부에서 TLS로 관리된다고 가정
	//  - PushLock/PopLock에서 현재 스레드의 top과 비교하여 순서 검증
};

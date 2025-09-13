#pragma once
#include <vector>
#include <map>
#include <stack>

// ����: �����ο��� unordered_map / set ���.
// (���⼭�� ���� �ڵ� ����: include�� �ǵ帮�� ����)

/*-----------------------------------------
 * DeadLockProfiler
 * - �� ȹ��/���� ������ �˷��ָ�
 *   "�� ���� �׷���"�� �����ϰ� ��� ����Ŭ �˻�
 * - ��� ����(��):
 *     PushLock("A");  // A �� ȹ�� ����
 *     PushLock("B");  // A ���� ���¿��� B���� ȹ�� �� A->B ���� ���
 *     PopLock("B");   // B ����
 *     PopLock("A");   // A ����
 * - ����(prev->curr)�� ���� ���� ������ DFS�� ��ȯ Ž��
 *   �� ���� �� ���� ����� ���� �ľǿ� ����
 ------------------------------------------*/
class DeadLockProfiler
{
public:
	// �� ȹ�� �˸�: ���� �׷���(prev->curr) ���� �� ����Ŭ �˻�
	void PushLock(const char* name);

	// �� ���� �˸�: ���� �������� ȹ�� ���ð� ��ġ�ϴ��� ����
	void PopLock(const char* name);

	// ��ü �׷����� ���� DFS�� ����Ŭ �˻�(���� ȣ���)
	void CheckCycle();

private:
	// DFS �� ��� ���� (�鿡�� Ž�� �� CRASH)
	void Dfs(int32 index);

private:
	// �̸� �� ID (���� ���� ��ȣ �ű�)
	unordered_map<const char*, int32> _nameToId;

	// ID �� �̸� (����Ŭ ��¿�)
	unordered_map<int32, const char*> _idToName;

	// ���İ��� �׷���: fromID �� { toID... }
	map<int32, set<int32>> _lockHistory;

	// ���� ��ȣ�� ���ؽ� (�׷���/��/������ ����ȭ)
	Mutex _lock;

private:
	// DFS ���� �迭��
	vector<int32>  _discoveredOrder;   // ���� �湮 ����(-1=�̹湮)
	int32          _discoveredCount = 0;
	vector<bool>   _finished;          // ��ó�� �Ϸ� ����
	vector<int32>  _parent;            // DFS Ʈ���� �θ�

	// �����庰 �� ����(LLockStack)�� �ܺο��� TLS�� �����ȴٰ� ����
	//  - PushLock/PopLock���� ���� �������� top�� ���Ͽ� ���� ����
};

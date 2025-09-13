#pragma once

// �ܼ� malloc/free ��� �⺻ �Ҵ���
class BaseAllocator
{
public:
	// �޸� �Ҵ� (malloc)
	static void* Alloc(int32 size);

	// �޸� ���� (free)
	static void Release(void* ptr);
};

// ������ ���� �Ҵ����� ������ Ž����
class StompAllocator
{
	enum { PAGE_SIZE = 0x1000 }; // �� ������ ũ�� (4KB)

public:
	// VirtualAlloc ���, ������
	static void* Alloc(int32 size);

	// VirtualFree ���, ��ü ������ ��ȯ
	static void Release(void* ptr);
};

// �޸� Ǯ ���, �������� ���� ���
class PoolAllocator
{
public:
	// �޸� Ǯ���� ������
	static void* Alloc(int32 size);

	// �޸𸮸� �ٽ� Ǯ�� �ݳ�
	static void Release(void* ptr);
};

// STL �����̳� ���� Ŀ���� allocator
template<typename T>
class StlAllocator
{
public:
	using value_type = T;

	StlAllocator() {}
	template<typename Other>
	StlAllocator(const StlAllocator<Other>&) {}

	// ��� ���� �� sizeof(T) ��ŭ �޸� Ȯ��
	T* allocate(size_t count)
	{
		const int32 size = static_cast<int32>(count * sizeof(T));
		return static_cast<T*>(PoolAllocator::Alloc(size));
	}

	// Ȯ���� �޸� ����
	void deallocate(T* ptr, size_t count)
	{
		PoolAllocator::Release(ptr);
	}
};

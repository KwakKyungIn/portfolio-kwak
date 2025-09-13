#pragma once
#include "Types.h"

// --------------------------------------------
// ��Ÿ���α׷��� ��ƿ
// - RTTI(typeid, dynamic_cast) ���� Ÿ�� ��ȯ ����
// - ���ø��� �̿��� Ÿ�� ���, ��ġ, ��ȯ ���� ���� üũ
// - ���� ���� ��ü ĳ���� ����ȭ / Ŀ���� ����Ʈ�����Ϳ� �Բ� ���
// --------------------------------------------

// ----------------------
// TypeList: Ÿ�� ����
// ----------------------
template<typename... T>
struct TypeList;

// 2���� ���� ��
template<typename T, typename U>
struct TypeList<T, U>
{
	using Head = T;
	using Tail = U;
};

// ���� �� ���� �� (���)
template<typename T, typename... U>
struct TypeList<T, U...>
{
	using Head = T;
	using Tail = TypeList<U...>;
};

// ----------------------
// Length: TypeList ����
// ----------------------
template<typename T>
struct Length;

template<>
struct Length<TypeList<>>
{
	enum { value = 0 };
};

template<typename T, typename... U>
struct Length<TypeList<T, U...>>
{
	enum { value = 1 + Length<TypeList<U...>>::value };
};

// ----------------------
// TypeAt: Ư�� ��ġ Ÿ��
// ----------------------
template<typename TL, int32 index>
struct TypeAt;

template<typename Head, typename... Tail>
struct TypeAt<TypeList<Head, Tail...>, 0>
{
	using Result = Head;
};

template<typename Head, typename... Tail, int32 index>
struct TypeAt<TypeList<Head, Tail...>, index>
{
	using Result = typename TypeAt<TypeList<Tail...>, index - 1>::Result;
};

// ----------------------
// IndexOf: Ÿ�� ��ġ ã��
// ----------------------
template<typename TL, typename T>
struct IndexOf;

template<typename... Tail, typename T>
struct IndexOf<TypeList<T, Tail...>, T>
{
	enum { value = 0 };
};

template<typename T>
struct IndexOf<TypeList<>, T>
{
	enum { value = -1 };
};

template<typename Head, typename... Tail, typename T>
struct IndexOf<TypeList<Head, Tail...>, T>
{
private:
	enum { temp = IndexOf<TypeList<Tail...>, T>::value };

public:
	enum { value = (temp == -1) ? -1 : temp + 1 };
};

// ----------------------
// Conversion: ��ȯ ����?
// ----------------------
template<typename From, typename To>
class Conversion
{
private:
	using Small = __int8;
	using Big = __int32;

	static Small Test(const To&) { return 0; }
	static Big Test(...) { return 0; }
	static From MakeFrom() { return 0; }

public:
	// exists = true �� ��ȯ ����
	enum { exists = sizeof(Test(MakeFrom())) == sizeof(Small) };
};

// ----------------------
// TypeConversion Table
// ----------------------
template<int32 v>
struct Int2Type
{
	enum { value = v };
};

template<typename TL>
class TypeConversion
{
public:
	enum { length = Length<TL>::value };

	TypeConversion()
	{
		// ��ȯ ���� ���̺� ����
		MakeTable(Int2Type<0>(), Int2Type<0>());
	}

	// i��° �� j��° ��ȯ ���� ����
	template<int32 i, int32 j>
	static void MakeTable(Int2Type<i>, Int2Type<j>)
	{
		using FromType = typename TypeAt<TL, i>::Result;
		using ToType = typename TypeAt<TL, j>::Result;

		if (Conversion<const FromType*, const ToType*>::exists)
			s_convert[i][j] = true;
		else
			s_convert[i][j] = false;

		MakeTable(Int2Type<i>(), Int2Type<j + 1>());
	}

	// j �� �� ���� i��
	template<int32 i>
	static void MakeTable(Int2Type<i>, Int2Type<length>)
	{
		MakeTable(Int2Type<i + 1>(), Int2Type<0>());
	}

	// i �� �� ����
	template<int j>
	static void MakeTable(Int2Type<length>, Int2Type<j>) {}

	// from �� to ��ȯ ����?
	static inline bool CanConvert(int32 from, int32 to)
	{
		static TypeConversion conversion;
		return s_convert[from][to];
	}

public:
	static bool s_convert[length][length]; // ��ȯ ���� ���̺�
};

template<typename TL>
bool TypeConversion<TL>::s_convert[length][length];

// ----------------------
// TypeCast & CanCast
// ----------------------
template<typename To, typename From>
To TypeCast(From* ptr)
{
	if (ptr == nullptr)
		return nullptr;

	using TL = typename From::TL;

	if (TypeConversion<TL>::CanConvert(ptr->_typeId, IndexOf<TL, remove_pointer_t<To>>::value))
		return static_cast<To>(ptr);

	return nullptr;
}

template<typename To, typename From>
shared_ptr<To> TypeCast(shared_ptr<From> ptr)
{
	if (ptr == nullptr)
		return nullptr;

	using TL = typename From::TL;

	if (TypeConversion<TL>::CanConvert(ptr->_typeId, IndexOf<TL, remove_pointer_t<To>>::value))
		return static_pointer_cast<To>(ptr);

	return nullptr;
}

template<typename To, typename From>
bool CanCast(From* ptr)
{
	if (ptr == nullptr)
		return false;

	using TL = typename From::TL;
	return TypeConversion<TL>::CanConvert(ptr->_typeId, IndexOf<TL, remove_pointer_t<To>>::value);
}

template<typename To, typename From>
bool CanCast(shared_ptr<From> ptr)
{
	if (ptr == nullptr)
		return false;

	using TL = typename From::TL;
	return TypeConversion<TL>::CanConvert(ptr->_typeId, IndexOf<TL, remove_pointer_t<To>>::value);
}

// ��ũ��: Ŭ������ Ÿ�Ը���Ʈ ���� / �ʱ�ȭ
#define DECLARE_TL   using TL = TL; int32 _typeId;
#define INIT_TL(Type) _typeId = IndexOf<TL, Type>::value;

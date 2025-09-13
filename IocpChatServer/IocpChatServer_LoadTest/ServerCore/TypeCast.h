#pragma once
#include "Types.h"

// --------------------------------------------
// 메타프로그래밍 유틸
// - RTTI(typeid, dynamic_cast) 없이 타입 변환 구현
// - 템플릿을 이용해 타입 목록, 위치, 변환 가능 여부 체크
// - 게임 서버 객체 캐스팅 최적화 / 커스텀 스마트포인터와 함께 사용
// --------------------------------------------

// ----------------------
// TypeList: 타입 모음
// ----------------------
template<typename... T>
struct TypeList;

// 2개만 있을 때
template<typename T, typename U>
struct TypeList<T, U>
{
	using Head = T;
	using Tail = U;
};

// 여러 개 있을 때 (재귀)
template<typename T, typename... U>
struct TypeList<T, U...>
{
	using Head = T;
	using Tail = TypeList<U...>;
};

// ----------------------
// Length: TypeList 길이
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
// TypeAt: 특정 위치 타입
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
// IndexOf: 타입 위치 찾기
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
// Conversion: 변환 가능?
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
	// exists = true → 변환 가능
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
		// 변환 가능 테이블 빌드
		MakeTable(Int2Type<0>(), Int2Type<0>());
	}

	// i번째 → j번째 변환 가능 여부
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

	// j 끝 → 다음 i로
	template<int32 i>
	static void MakeTable(Int2Type<i>, Int2Type<length>)
	{
		MakeTable(Int2Type<i + 1>(), Int2Type<0>());
	}

	// i 끝 → 종료
	template<int j>
	static void MakeTable(Int2Type<length>, Int2Type<j>) {}

	// from → to 변환 가능?
	static inline bool CanConvert(int32 from, int32 to)
	{
		static TypeConversion conversion;
		return s_convert[from][to];
	}

public:
	static bool s_convert[length][length]; // 변환 가능 테이블
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

// 매크로: 클래스에 타입리스트 선언 / 초기화
#define DECLARE_TL   using TL = TL; int32 _typeId;
#define INIT_TL(Type) _typeId = IndexOf<TL, Type>::value;

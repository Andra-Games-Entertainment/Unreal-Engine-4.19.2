// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Traits/ElementType.h"

namespace AlgoImpl
{
	template <typename RangeType, typename PredicateType>
	const typename TElementType<RangeType>::Type* FindByPredicate(const RangeType& Range, PredicateType Predicate)
	{
		for (const auto& Elem : Range)
		{
			if (Predicate(Elem))
			{
				return &Elem;
			}
		}

		return nullptr;
	}
}

namespace Algo
{
	/**
	 * Returns a pointer to the first element matching a value in a range.
	 *
	 * @param  Range  The range to search.
	 * @param  Value  The value to search for.
	 * @return A pointer to the first element found, or nullptr if none was found.
	 */
	template <typename RangeType, typename ValueType>
	FORCEINLINE typename TElementType<RangeType>::Type* Find(RangeType& Range, const ValueType& Value)
	{
		return const_cast<typename TElementType<RangeType>::Type*>(Find(const_cast<const RangeType&>(Range), Value));
	}

	/**
	 * Returns a pointer to the first element matching a value in a range.
	 *
	 * @param  Range  The range to search.
	 * @param  Value  The value to search for.
	 * @return A pointer to the first element found, or nullptr if none was found.
	 */
	template <typename RangeType, typename ValueType>
	FORCEINLINE const typename TElementType<RangeType>::Type* Find(const RangeType& Range, const ValueType& Value)
	{
		return AlgoImpl::FindByPredicate(Range, [&](const typename TElementType<RangeType>::Type& Elem){
			return Elem == Value;
		});
	}

	/**
	 * Returns a pointer to the first element matching a predicate in a range.
	 *
	 * @param  Range  The range to search.
	 * @param  Pred   The predicate to search for.
	 * @return A pointer to the first element found, or nullptr if none was found.
	 */
	template <typename RangeType, typename PredicateType>
	FORCEINLINE typename TElementType<RangeType>::Type* FindByPredicate(RangeType& Range, PredicateType Pred)
	{
		return const_cast<typename TElementType<RangeType>::Type*>(FindByPredicate(const_cast<const RangeType&>(Range), Pred));
	}

	/**
	 * Returns a pointer to the first element matching a predicate in a range.
	 *
	 * @param  Range  The range to search.
	 * @param  Pred   The predicate to search for.
	 * @return A pointer to the first element found, or nullptr if none was found.
	 */
	template <typename RangeType, typename PredicateType>
	FORCEINLINE const typename TElementType<RangeType>::Type* FindByPredicate(const RangeType& Range, PredicateType Pred)
	{
		return AlgoImpl::FindByPredicate(Range, Pred);
	}
}

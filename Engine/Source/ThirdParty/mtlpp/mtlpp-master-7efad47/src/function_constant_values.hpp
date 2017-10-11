/*
 * Copyright 2016-2017 Nikolay Aleksiev. All rights reserved.
 * License: https://github.com/naleksiev/mtlpp/blob/master/LICENSE
 */
// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Modifications for Unreal Engine

#pragma once

#include "defines.hpp"
#include "ns.hpp"
#include "argument.hpp"

MTLPP_CLASS(MTLFunctionConstantValues);

namespace mtlpp
{
    class FunctionConstantValues : public ns::Object<MTLFunctionConstantValues*>
    {
    public:
        FunctionConstantValues();
        FunctionConstantValues(MTLFunctionConstantValues* handle) : ns::Object<MTLFunctionConstantValues*>(handle) { }

        void SetConstantValue(const void* value, DataType type, uint32_t index);
        void SetConstantValue(const void* value, DataType type, const ns::String& name);
        void SetConstantValues(const void* value, DataType type, const ns::Range& range);

        void Reset();
    }
    MTLPP_AVAILABLE(10_12, 10_0);
}

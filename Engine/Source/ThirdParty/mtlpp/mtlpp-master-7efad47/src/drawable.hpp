/*
 * Copyright 2016-2017 Nikolay Aleksiev. All rights reserved.
 * License: https://github.com/naleksiev/mtlpp/blob/master/LICENSE
 */
// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Modifications for Unreal Engine

#pragma once

#include "defines.hpp"
#include "ns.hpp"

MTLPP_PROTOCOL(MTLDrawable);

namespace mtlpp
{
    class Drawable : public ns::Object<ns::Protocol<id<MTLDrawable>>::type>
    {
    public:
        Drawable() { }
        Drawable(ns::Protocol<id<MTLDrawable>>::type handle) : ns::Object<ns::Protocol<id<MTLDrawable>>::type>(handle) { }

        double   GetPresentedTime() const MTLPP_AVAILABLE_IOS(10_3);
        uint64_t GetDrawableID() const MTLPP_AVAILABLE_IOS(10_3);

        void Present();
        void PresentAtTime(double presentationTime);
        void PresentAfterMinimumDuration(double duration) MTLPP_AVAILABLE_IOS(10_3);
        void AddPresentedHandler(std::function<void(const Drawable&)> handler) MTLPP_AVAILABLE_IOS(10_3);
    }
    MTLPP_AVAILABLE(10_11, 8_0);
}


// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_SYSTEMPERMISSION_H
#define OVR_SYSTEMPERMISSION_H

#include "OVR_Platform_Defs.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct ovrSystemPermission *ovrSystemPermissionHandle;

OVRP_PUBLIC_FUNCTION(bool) ovr_SystemPermission_GetHasPermission(const ovrSystemPermissionHandle obj);

#endif

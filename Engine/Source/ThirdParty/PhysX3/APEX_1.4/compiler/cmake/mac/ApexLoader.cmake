#
# Build APEX_Loader
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

SET(AM_SOURCE_DIR ${APEX_MODULE_DIR}/loader)

SET(APEX_LOADER_LIBTYPE SHARED)


# Use generator expressions to set config specific preprocessor definitions
SET(APEX_LOADER_COMPILE_DEFS 

	# Common to all configurations
	${APEX_MAC_COMPILE_DEFS};__Mac__;Mac;_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;NX_USE_SDK_STATICLIBS;NX_FOUNDATION_STATICLIB;NV_PARAMETERIZED_HIDE_DESCRIPTIONS=1;_LIB;ENABLE_TEST=0;

	$<$<CONFIG:debug>:${APEX_MAC_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${APEX_MAC_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${APEX_MAC_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${APEX_MAC_RELEASE_COMPILE_DEFS};>
)


# include common ApexLoader.cmake
INCLUDE(../common/ApexLoader.cmake)

# Do final direct sets after the target has been defined
TARGET_LINK_LIBRARIES(APEX_Loader PUBLIC 
	${NVTOOLSEXT_LIBRARIES}
	APEX_Clothing APEX_Destructible APEX_Legacy
	ApexCommon ApexFramework ApexShared NvParameterized PsFastXml PxFoundation PxPvdSDK
)

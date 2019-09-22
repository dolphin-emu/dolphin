#ifndef OPENXR_REFLECTION_H_
#define OPENXR_REFLECTION_H_ 1

/*
** Copyright (c) 2017-2019 The Khronos Group Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/*
** This header is generated from the Khronos OpenXR XML API Registry.
**
*/

#include "openxr.h"

/*
This file contains expansion macros (X Macros) for OpenXR enumerations and structures.
Example of how to use expansion macros to make an enum-to-string function:

#define XR_ENUM_CASE_STR(name, val) case name: return #name;
#define XR_ENUM_STR(enumType)                         \
    constexpr const char* XrEnumStr(enumType e) {     \
        switch (e) {                                  \
            XR_LIST_ENUM_##enumType(XR_ENUM_CASE_STR) \
            default: return "Unknown";                \
        }                                             \
    }                                                 \

XR_ENUM_STR(XrResult);
*/

#define XR_LIST_ENUM_XrResult(_) \
    _(XR_SUCCESS, 0) \
    _(XR_TIMEOUT_EXPIRED, 1) \
    _(XR_SESSION_LOSS_PENDING, 3) \
    _(XR_EVENT_UNAVAILABLE, 4) \
    _(XR_SPACE_BOUNDS_UNAVAILABLE, 7) \
    _(XR_SESSION_NOT_FOCUSED, 8) \
    _(XR_FRAME_DISCARDED, 9) \
    _(XR_ERROR_VALIDATION_FAILURE, -1) \
    _(XR_ERROR_RUNTIME_FAILURE, -2) \
    _(XR_ERROR_OUT_OF_MEMORY, -3) \
    _(XR_ERROR_API_VERSION_UNSUPPORTED, -4) \
    _(XR_ERROR_INITIALIZATION_FAILED, -6) \
    _(XR_ERROR_FUNCTION_UNSUPPORTED, -7) \
    _(XR_ERROR_FEATURE_UNSUPPORTED, -8) \
    _(XR_ERROR_EXTENSION_NOT_PRESENT, -9) \
    _(XR_ERROR_LIMIT_REACHED, -10) \
    _(XR_ERROR_SIZE_INSUFFICIENT, -11) \
    _(XR_ERROR_HANDLE_INVALID, -12) \
    _(XR_ERROR_INSTANCE_LOST, -13) \
    _(XR_ERROR_SESSION_RUNNING, -14) \
    _(XR_ERROR_SESSION_NOT_RUNNING, -16) \
    _(XR_ERROR_SESSION_LOST, -17) \
    _(XR_ERROR_SYSTEM_INVALID, -18) \
    _(XR_ERROR_PATH_INVALID, -19) \
    _(XR_ERROR_PATH_COUNT_EXCEEDED, -20) \
    _(XR_ERROR_PATH_FORMAT_INVALID, -21) \
    _(XR_ERROR_PATH_UNSUPPORTED, -22) \
    _(XR_ERROR_LAYER_INVALID, -23) \
    _(XR_ERROR_LAYER_LIMIT_EXCEEDED, -24) \
    _(XR_ERROR_SWAPCHAIN_RECT_INVALID, -25) \
    _(XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED, -26) \
    _(XR_ERROR_ACTION_TYPE_MISMATCH, -27) \
    _(XR_ERROR_SESSION_NOT_READY, -28) \
    _(XR_ERROR_SESSION_NOT_STOPPING, -29) \
    _(XR_ERROR_TIME_INVALID, -30) \
    _(XR_ERROR_REFERENCE_SPACE_UNSUPPORTED, -31) \
    _(XR_ERROR_FILE_ACCESS_ERROR, -32) \
    _(XR_ERROR_FILE_CONTENTS_INVALID, -33) \
    _(XR_ERROR_FORM_FACTOR_UNSUPPORTED, -34) \
    _(XR_ERROR_FORM_FACTOR_UNAVAILABLE, -35) \
    _(XR_ERROR_API_LAYER_NOT_PRESENT, -36) \
    _(XR_ERROR_CALL_ORDER_INVALID, -37) \
    _(XR_ERROR_GRAPHICS_DEVICE_INVALID, -38) \
    _(XR_ERROR_POSE_INVALID, -39) \
    _(XR_ERROR_INDEX_OUT_OF_RANGE, -40) \
    _(XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED, -41) \
    _(XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED, -42) \
    _(XR_ERROR_NAME_DUPLICATED, -44) \
    _(XR_ERROR_NAME_INVALID, -45) \
    _(XR_ERROR_ACTIONSET_NOT_ATTACHED, -46) \
    _(XR_ERROR_ACTIONSETS_ALREADY_ATTACHED, -47) \
    _(XR_ERROR_LOCALIZED_NAME_DUPLICATED, -48) \
    _(XR_ERROR_LOCALIZED_NAME_INVALID, -49) \
    _(XR_ERROR_ANDROID_THREAD_SETTINGS_ID_INVALID_KHR, -1000003000) \
    _(XR_ERROR_ANDROID_THREAD_SETTINGS_FAILURE_KHR, -1000003001) \
    _(XR_ERROR_CREATE_SPATIAL_ANCHOR_FAILED_MSFT, -1000039001) \
    _(XR_RESULT_MAX_ENUM, 0x7FFFFFFF)

#define XR_LIST_ENUM_XrStructureType(_) \
    _(XR_TYPE_UNKNOWN, 0) \
    _(XR_TYPE_API_LAYER_PROPERTIES, 1) \
    _(XR_TYPE_EXTENSION_PROPERTIES, 2) \
    _(XR_TYPE_INSTANCE_CREATE_INFO, 3) \
    _(XR_TYPE_SYSTEM_GET_INFO, 4) \
    _(XR_TYPE_SYSTEM_PROPERTIES, 5) \
    _(XR_TYPE_VIEW_LOCATE_INFO, 6) \
    _(XR_TYPE_VIEW, 7) \
    _(XR_TYPE_SESSION_CREATE_INFO, 8) \
    _(XR_TYPE_SWAPCHAIN_CREATE_INFO, 9) \
    _(XR_TYPE_SESSION_BEGIN_INFO, 10) \
    _(XR_TYPE_VIEW_STATE, 11) \
    _(XR_TYPE_FRAME_END_INFO, 12) \
    _(XR_TYPE_HAPTIC_VIBRATION, 13) \
    _(XR_TYPE_EVENT_DATA_BUFFER, 16) \
    _(XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING, 17) \
    _(XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED, 18) \
    _(XR_TYPE_ACTION_STATE_BOOLEAN, 23) \
    _(XR_TYPE_ACTION_STATE_FLOAT, 24) \
    _(XR_TYPE_ACTION_STATE_VECTOR2F, 25) \
    _(XR_TYPE_ACTION_STATE_POSE, 27) \
    _(XR_TYPE_ACTION_SET_CREATE_INFO, 28) \
    _(XR_TYPE_ACTION_CREATE_INFO, 29) \
    _(XR_TYPE_INSTANCE_PROPERTIES, 32) \
    _(XR_TYPE_FRAME_WAIT_INFO, 33) \
    _(XR_TYPE_COMPOSITION_LAYER_PROJECTION, 35) \
    _(XR_TYPE_COMPOSITION_LAYER_QUAD, 36) \
    _(XR_TYPE_REFERENCE_SPACE_CREATE_INFO, 37) \
    _(XR_TYPE_ACTION_SPACE_CREATE_INFO, 38) \
    _(XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING, 40) \
    _(XR_TYPE_VIEW_CONFIGURATION_VIEW, 41) \
    _(XR_TYPE_SPACE_LOCATION, 42) \
    _(XR_TYPE_SPACE_VELOCITY, 43) \
    _(XR_TYPE_FRAME_STATE, 44) \
    _(XR_TYPE_VIEW_CONFIGURATION_PROPERTIES, 45) \
    _(XR_TYPE_FRAME_BEGIN_INFO, 46) \
    _(XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW, 48) \
    _(XR_TYPE_EVENT_DATA_EVENTS_LOST, 49) \
    _(XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING, 51) \
    _(XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED, 52) \
    _(XR_TYPE_INTERACTION_PROFILE_STATE, 53) \
    _(XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, 55) \
    _(XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, 56) \
    _(XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, 57) \
    _(XR_TYPE_ACTION_STATE_GET_INFO, 58) \
    _(XR_TYPE_HAPTIC_ACTION_INFO, 59) \
    _(XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO, 60) \
    _(XR_TYPE_ACTIONS_SYNC_INFO, 61) \
    _(XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO, 62) \
    _(XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO, 63) \
    _(XR_TYPE_COMPOSITION_LAYER_CUBE_KHR, 1000006000) \
    _(XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR, 1000008000) \
    _(XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR, 1000010000) \
    _(XR_TYPE_VULKAN_SWAPCHAIN_FORMAT_LIST_CREATE_INFO_KHR, 1000014000) \
    _(XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT, 1000015000) \
    _(XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR, 1000017000) \
    _(XR_TYPE_COMPOSITION_LAYER_EQUIRECT_KHR, 1000018000) \
    _(XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, 1000019000) \
    _(XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT, 1000019001) \
    _(XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT, 1000019002) \
    _(XR_TYPE_DEBUG_UTILS_LABEL_EXT, 1000019003) \
    _(XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR, 1000023000) \
    _(XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR, 1000023001) \
    _(XR_TYPE_GRAPHICS_BINDING_OPENGL_XCB_KHR, 1000023002) \
    _(XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR, 1000023003) \
    _(XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, 1000023004) \
    _(XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR, 1000023005) \
    _(XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR, 1000024001) \
    _(XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR, 1000024002) \
    _(XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR, 1000024003) \
    _(XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR, 1000025000) \
    _(XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR, 1000025001) \
    _(XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR, 1000025002) \
    _(XR_TYPE_GRAPHICS_BINDING_D3D11_KHR, 1000027000) \
    _(XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR, 1000027001) \
    _(XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR, 1000027002) \
    _(XR_TYPE_GRAPHICS_BINDING_D3D12_KHR, 1000028000) \
    _(XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR, 1000028001) \
    _(XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR, 1000028002) \
    _(XR_TYPE_VISIBILITY_MASK_KHR, 1000031000) \
    _(XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR, 1000031001) \
    _(XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_MSFT, 1000039000) \
    _(XR_TYPE_SPATIAL_ANCHOR_SPACE_CREATE_INFO_MSFT, 1000039001) \
    _(XR_STRUCTURE_TYPE_MAX_ENUM, 0x7FFFFFFF)

#define XR_LIST_ENUM_XrFormFactor(_) \
    _(XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY, 1) \
    _(XR_FORM_FACTOR_HANDHELD_DISPLAY, 2) \
    _(XR_FORM_FACTOR_MAX_ENUM, 0x7FFFFFFF)

#define XR_LIST_ENUM_XrViewConfigurationType(_) \
    _(XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO, 1) \
    _(XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2) \
    _(XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO, 1000037000) \
    _(XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM, 0x7FFFFFFF)

#define XR_LIST_ENUM_XrEnvironmentBlendMode(_) \
    _(XR_ENVIRONMENT_BLEND_MODE_OPAQUE, 1) \
    _(XR_ENVIRONMENT_BLEND_MODE_ADDITIVE, 2) \
    _(XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND, 3) \
    _(XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM, 0x7FFFFFFF)

#define XR_LIST_ENUM_XrReferenceSpaceType(_) \
    _(XR_REFERENCE_SPACE_TYPE_VIEW, 1) \
    _(XR_REFERENCE_SPACE_TYPE_LOCAL, 2) \
    _(XR_REFERENCE_SPACE_TYPE_STAGE, 3) \
    _(XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT, 1000038000) \
    _(XR_REFERENCE_SPACE_TYPE_MAX_ENUM, 0x7FFFFFFF)

#define XR_LIST_ENUM_XrActionType(_) \
    _(XR_ACTION_TYPE_BOOLEAN_INPUT, 1) \
    _(XR_ACTION_TYPE_FLOAT_INPUT, 2) \
    _(XR_ACTION_TYPE_VECTOR2F_INPUT, 3) \
    _(XR_ACTION_TYPE_POSE_INPUT, 4) \
    _(XR_ACTION_TYPE_VIBRATION_OUTPUT, 100) \
    _(XR_ACTION_TYPE_MAX_ENUM, 0x7FFFFFFF)

#define XR_LIST_ENUM_XrEyeVisibility(_) \
    _(XR_EYE_VISIBILITY_BOTH, 0) \
    _(XR_EYE_VISIBILITY_LEFT, 1) \
    _(XR_EYE_VISIBILITY_RIGHT, 2) \
    _(XR_EYE_VISIBILITY_MAX_ENUM, 0x7FFFFFFF)

#define XR_LIST_ENUM_XrSessionState(_) \
    _(XR_SESSION_STATE_UNKNOWN, 0) \
    _(XR_SESSION_STATE_IDLE, 1) \
    _(XR_SESSION_STATE_READY, 2) \
    _(XR_SESSION_STATE_SYNCHRONIZED, 3) \
    _(XR_SESSION_STATE_VISIBLE, 4) \
    _(XR_SESSION_STATE_FOCUSED, 5) \
    _(XR_SESSION_STATE_STOPPING, 6) \
    _(XR_SESSION_STATE_LOSS_PENDING, 7) \
    _(XR_SESSION_STATE_EXITING, 8) \
    _(XR_SESSION_STATE_MAX_ENUM, 0x7FFFFFFF)

#define XR_LIST_ENUM_XrObjectType(_) \
    _(XR_OBJECT_TYPE_UNKNOWN, 0) \
    _(XR_OBJECT_TYPE_INSTANCE, 1) \
    _(XR_OBJECT_TYPE_SESSION, 2) \
    _(XR_OBJECT_TYPE_SWAPCHAIN, 3) \
    _(XR_OBJECT_TYPE_SPACE, 4) \
    _(XR_OBJECT_TYPE_ACTION_SET, 5) \
    _(XR_OBJECT_TYPE_ACTION, 6) \
    _(XR_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT, 1000019000) \
    _(XR_OBJECT_TYPE_SPATIAL_ANCHOR_MSFT, 1000039000) \
    _(XR_OBJECT_TYPE_MAX_ENUM, 0x7FFFFFFF)

#define XR_LIST_ENUM_XrAndroidThreadTypeKHR(_) \
    _(XR_ANDROID_THREAD_TYPE_APPLICATION_MAIN_KHR, 1) \
    _(XR_ANDROID_THREAD_TYPE_APPLICATION_WORKER_KHR, 2) \
    _(XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR, 3) \
    _(XR_ANDROID_THREAD_TYPE_RENDERER_WORKER_KHR, 4) \
    _(XR_ANDROID_THREAD_TYPE_MAX_ENUM_KHR, 0x7FFFFFFF)

#define XR_LIST_ENUM_XrVisibilityMaskTypeKHR(_) \
    _(XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR, 1) \
    _(XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR, 2) \
    _(XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR, 3) \
    _(XR_VISIBILITY_MASK_TYPE_MAX_ENUM_KHR, 0x7FFFFFFF)

#define XR_LIST_ENUM_XrPerfSettingsDomainEXT(_) \
    _(XR_PERF_SETTINGS_DOMAIN_CPU_EXT, 1) \
    _(XR_PERF_SETTINGS_DOMAIN_GPU_EXT, 2) \
    _(XR_PERF_SETTINGS_DOMAIN_MAX_ENUM_EXT, 0x7FFFFFFF)

#define XR_LIST_ENUM_XrPerfSettingsSubDomainEXT(_) \
    _(XR_PERF_SETTINGS_SUB_DOMAIN_COMPOSITING_EXT, 1) \
    _(XR_PERF_SETTINGS_SUB_DOMAIN_RENDERING_EXT, 2) \
    _(XR_PERF_SETTINGS_SUB_DOMAIN_THERMAL_EXT, 3) \
    _(XR_PERF_SETTINGS_SUB_DOMAIN_MAX_ENUM_EXT, 0x7FFFFFFF)

#define XR_LIST_ENUM_XrPerfSettingsLevelEXT(_) \
    _(XR_PERF_SETTINGS_LEVEL_POWER_SAVINGS_EXT, 0) \
    _(XR_PERF_SETTINGS_LEVEL_SUSTAINED_LOW_EXT, 25) \
    _(XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT, 50) \
    _(XR_PERF_SETTINGS_LEVEL_BOOST_EXT, 75) \
    _(XR_PERF_SETTINGS_LEVEL_MAX_ENUM_EXT, 0x7FFFFFFF)

#define XR_LIST_ENUM_XrPerfSettingsNotificationLevelEXT(_) \
    _(XR_PERF_SETTINGS_NOTIF_LEVEL_NORMAL_EXT, 0) \
    _(XR_PERF_SETTINGS_NOTIF_LEVEL_WARNING_EXT, 25) \
    _(XR_PERF_SETTINGS_NOTIF_LEVEL_IMPAIRED_EXT, 75) \
    _(XR_PERF_SETTINGS_NOTIFICATION_LEVEL_MAX_ENUM_EXT, 0x7FFFFFFF)

#define XR_LIST_BITS_XrInstanceCreateFlags(_)

#define XR_LIST_BITS_XrSessionCreateFlags(_)

#define XR_LIST_BITS_XrSpaceVelocityFlags(_) \
    _(XR_SPACE_VELOCITY_LINEAR_VALID_BIT, 0x00000001) \
    _(XR_SPACE_VELOCITY_ANGULAR_VALID_BIT, 0x00000002)

#define XR_LIST_BITS_XrSpaceLocationFlags(_) \
    _(XR_SPACE_LOCATION_ORIENTATION_VALID_BIT, 0x00000001) \
    _(XR_SPACE_LOCATION_POSITION_VALID_BIT, 0x00000002) \
    _(XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT, 0x00000004) \
    _(XR_SPACE_LOCATION_POSITION_TRACKED_BIT, 0x00000008)

#define XR_LIST_BITS_XrSwapchainCreateFlags(_) \
    _(XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT, 0x00000001) \
    _(XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT, 0x00000002)

#define XR_LIST_BITS_XrSwapchainUsageFlags(_) \
    _(XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, 0x00000001) \
    _(XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0x00000002) \
    _(XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT, 0x00000004) \
    _(XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT, 0x00000008) \
    _(XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, 0x00000010) \
    _(XR_SWAPCHAIN_USAGE_SAMPLED_BIT, 0x00000020) \
    _(XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT, 0x00000040)

#define XR_LIST_BITS_XrCompositionLayerFlags(_) \
    _(XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT, 0x00000001) \
    _(XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT, 0x00000002) \
    _(XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT, 0x00000004)

#define XR_LIST_BITS_XrViewStateFlags(_) \
    _(XR_VIEW_STATE_ORIENTATION_VALID_BIT, 0x00000001) \
    _(XR_VIEW_STATE_POSITION_VALID_BIT, 0x00000002) \
    _(XR_VIEW_STATE_ORIENTATION_TRACKED_BIT, 0x00000004) \
    _(XR_VIEW_STATE_POSITION_TRACKED_BIT, 0x00000008)

#define XR_LIST_BITS_XrInputSourceLocalizedNameFlags(_) \
    _(XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT, 0x00000001) \
    _(XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT, 0x00000002) \
    _(XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT, 0x00000004)

#define XR_LIST_BITS_XrDebugUtilsMessageSeverityFlagsEXT(_) \
    _(XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, 0x00000001) \
    _(XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, 0x00000010) \
    _(XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, 0x00000100) \
    _(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, 0x00001000)

#define XR_LIST_BITS_XrDebugUtilsMessageTypeFlagsEXT(_) \
    _(XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, 0x00000001) \
    _(XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, 0x00000002) \
    _(XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, 0x00000004) \
    _(XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT, 0x00000008)

#define XR_LIST_STRUCT_XrApiLayerProperties(_) \
    _(type) \
    _(next) \
    _(layerName) \
    _(specVersion) \
    _(layerVersion) \
    _(description)

#define XR_LIST_STRUCT_XrExtensionProperties(_) \
    _(type) \
    _(next) \
    _(extensionName) \
    _(extensionVersion)

#define XR_LIST_STRUCT_XrApplicationInfo(_) \
    _(applicationName) \
    _(applicationVersion) \
    _(engineName) \
    _(engineVersion) \
    _(apiVersion)

#define XR_LIST_STRUCT_XrInstanceCreateInfo(_) \
    _(type) \
    _(next) \
    _(createFlags) \
    _(applicationInfo) \
    _(enabledApiLayerCount) \
    _(enabledApiLayerNames) \
    _(enabledExtensionCount) \
    _(enabledExtensionNames)

#define XR_LIST_STRUCT_XrInstanceProperties(_) \
    _(type) \
    _(next) \
    _(runtimeVersion) \
    _(runtimeName)

#define XR_LIST_STRUCT_XrEventDataBuffer(_) \
    _(type) \
    _(next) \
    _(varying)

#define XR_LIST_STRUCT_XrSystemGetInfo(_) \
    _(type) \
    _(next) \
    _(formFactor)

#define XR_LIST_STRUCT_XrSystemGraphicsProperties(_) \
    _(maxSwapchainImageHeight) \
    _(maxSwapchainImageWidth) \
    _(maxLayerCount)

#define XR_LIST_STRUCT_XrSystemTrackingProperties(_) \
    _(orientationTracking) \
    _(positionTracking)

#define XR_LIST_STRUCT_XrSystemProperties(_) \
    _(type) \
    _(next) \
    _(systemId) \
    _(vendorId) \
    _(systemName) \
    _(graphicsProperties) \
    _(trackingProperties)

#define XR_LIST_STRUCT_XrSessionCreateInfo(_) \
    _(type) \
    _(next) \
    _(createFlags) \
    _(systemId)

#define XR_LIST_STRUCT_XrVector3f(_) \
    _(x) \
    _(y) \
    _(z)

#define XR_LIST_STRUCT_XrSpaceVelocity(_) \
    _(type) \
    _(next) \
    _(velocityFlags) \
    _(linearVelocity) \
    _(angularVelocity)

#define XR_LIST_STRUCT_XrQuaternionf(_) \
    _(x) \
    _(y) \
    _(z) \
    _(w)

#define XR_LIST_STRUCT_XrPosef(_) \
    _(orientation) \
    _(position)

#define XR_LIST_STRUCT_XrReferenceSpaceCreateInfo(_) \
    _(type) \
    _(next) \
    _(referenceSpaceType) \
    _(poseInReferenceSpace)

#define XR_LIST_STRUCT_XrExtent2Df(_) \
    _(width) \
    _(height)

#define XR_LIST_STRUCT_XrActionSpaceCreateInfo(_) \
    _(type) \
    _(next) \
    _(action) \
    _(subactionPath) \
    _(poseInActionSpace)

#define XR_LIST_STRUCT_XrSpaceLocation(_) \
    _(type) \
    _(next) \
    _(locationFlags) \
    _(pose)

#define XR_LIST_STRUCT_XrViewConfigurationProperties(_) \
    _(type) \
    _(next) \
    _(viewConfigurationType) \
    _(fovMutable)

#define XR_LIST_STRUCT_XrViewConfigurationView(_) \
    _(type) \
    _(next) \
    _(recommendedImageRectWidth) \
    _(maxImageRectWidth) \
    _(recommendedImageRectHeight) \
    _(maxImageRectHeight) \
    _(recommendedSwapchainSampleCount) \
    _(maxSwapchainSampleCount)

#define XR_LIST_STRUCT_XrSwapchainCreateInfo(_) \
    _(type) \
    _(next) \
    _(createFlags) \
    _(usageFlags) \
    _(format) \
    _(sampleCount) \
    _(width) \
    _(height) \
    _(faceCount) \
    _(arraySize) \
    _(mipCount)

#define XR_LIST_STRUCT_XrSwapchainImageBaseHeader(_) \
    _(type) \
    _(next)

#define XR_LIST_STRUCT_XrSwapchainImageAcquireInfo(_) \
    _(type) \
    _(next)

#define XR_LIST_STRUCT_XrSwapchainImageWaitInfo(_) \
    _(type) \
    _(next) \
    _(timeout)

#define XR_LIST_STRUCT_XrSwapchainImageReleaseInfo(_) \
    _(type) \
    _(next)

#define XR_LIST_STRUCT_XrSessionBeginInfo(_) \
    _(type) \
    _(next) \
    _(primaryViewConfigurationType)

#define XR_LIST_STRUCT_XrFrameWaitInfo(_) \
    _(type) \
    _(next)

#define XR_LIST_STRUCT_XrFrameState(_) \
    _(type) \
    _(next) \
    _(predictedDisplayTime) \
    _(predictedDisplayPeriod) \
    _(shouldRender)

#define XR_LIST_STRUCT_XrFrameBeginInfo(_) \
    _(type) \
    _(next)

#define XR_LIST_STRUCT_XrCompositionLayerBaseHeader(_) \
    _(type) \
    _(next) \
    _(layerFlags) \
    _(space)

#define XR_LIST_STRUCT_XrFrameEndInfo(_) \
    _(type) \
    _(next) \
    _(displayTime) \
    _(environmentBlendMode) \
    _(layerCount) \
    _(layers)

#define XR_LIST_STRUCT_XrViewLocateInfo(_) \
    _(type) \
    _(next) \
    _(viewConfigurationType) \
    _(displayTime) \
    _(space)

#define XR_LIST_STRUCT_XrViewState(_) \
    _(type) \
    _(next) \
    _(viewStateFlags)

#define XR_LIST_STRUCT_XrFovf(_) \
    _(angleLeft) \
    _(angleRight) \
    _(angleUp) \
    _(angleDown)

#define XR_LIST_STRUCT_XrView(_) \
    _(type) \
    _(next) \
    _(pose) \
    _(fov)

#define XR_LIST_STRUCT_XrActionSetCreateInfo(_) \
    _(type) \
    _(next) \
    _(actionSetName) \
    _(localizedActionSetName) \
    _(priority)

#define XR_LIST_STRUCT_XrActionCreateInfo(_) \
    _(type) \
    _(next) \
    _(actionName) \
    _(actionType) \
    _(countSubactionPaths) \
    _(subactionPaths) \
    _(localizedActionName)

#define XR_LIST_STRUCT_XrActionSuggestedBinding(_) \
    _(action) \
    _(binding)

#define XR_LIST_STRUCT_XrInteractionProfileSuggestedBinding(_) \
    _(type) \
    _(next) \
    _(interactionProfile) \
    _(countSuggestedBindings) \
    _(suggestedBindings)

#define XR_LIST_STRUCT_XrSessionActionSetsAttachInfo(_) \
    _(type) \
    _(next) \
    _(countActionSets) \
    _(actionSets)

#define XR_LIST_STRUCT_XrInteractionProfileState(_) \
    _(type) \
    _(next) \
    _(interactionProfile)

#define XR_LIST_STRUCT_XrActionStateGetInfo(_) \
    _(type) \
    _(next) \
    _(action) \
    _(subactionPath)

#define XR_LIST_STRUCT_XrActionStateBoolean(_) \
    _(type) \
    _(next) \
    _(currentState) \
    _(changedSinceLastSync) \
    _(lastChangeTime) \
    _(isActive)

#define XR_LIST_STRUCT_XrActionStateFloat(_) \
    _(type) \
    _(next) \
    _(currentState) \
    _(changedSinceLastSync) \
    _(lastChangeTime) \
    _(isActive)

#define XR_LIST_STRUCT_XrVector2f(_) \
    _(x) \
    _(y)

#define XR_LIST_STRUCT_XrActionStateVector2f(_) \
    _(type) \
    _(next) \
    _(currentState) \
    _(changedSinceLastSync) \
    _(lastChangeTime) \
    _(isActive)

#define XR_LIST_STRUCT_XrActionStatePose(_) \
    _(type) \
    _(next) \
    _(isActive)

#define XR_LIST_STRUCT_XrActiveActionSet(_) \
    _(actionSet) \
    _(subactionPath)

#define XR_LIST_STRUCT_XrActionsSyncInfo(_) \
    _(type) \
    _(next) \
    _(countActiveActionSets) \
    _(activeActionSets)

#define XR_LIST_STRUCT_XrBoundSourcesForActionEnumerateInfo(_) \
    _(type) \
    _(next) \
    _(action)

#define XR_LIST_STRUCT_XrInputSourceLocalizedNameGetInfo(_) \
    _(type) \
    _(next) \
    _(sourcePath) \
    _(whichComponents)

#define XR_LIST_STRUCT_XrHapticActionInfo(_) \
    _(type) \
    _(next) \
    _(action) \
    _(subactionPath)

#define XR_LIST_STRUCT_XrHapticBaseHeader(_) \
    _(type) \
    _(next)

#define XR_LIST_STRUCT_XrBaseInStructure(_) \
    _(type) \
    _(next)

#define XR_LIST_STRUCT_XrBaseOutStructure(_) \
    _(type) \
    _(next)

#define XR_LIST_STRUCT_XrOffset2Di(_) \
    _(x) \
    _(y)

#define XR_LIST_STRUCT_XrExtent2Di(_) \
    _(width) \
    _(height)

#define XR_LIST_STRUCT_XrRect2Di(_) \
    _(offset) \
    _(extent)

#define XR_LIST_STRUCT_XrSwapchainSubImage(_) \
    _(swapchain) \
    _(imageRect) \
    _(imageArrayIndex)

#define XR_LIST_STRUCT_XrCompositionLayerProjectionView(_) \
    _(type) \
    _(next) \
    _(pose) \
    _(fov) \
    _(subImage)

#define XR_LIST_STRUCT_XrCompositionLayerProjection(_) \
    _(type) \
    _(next) \
    _(layerFlags) \
    _(space) \
    _(viewCount) \
    _(views)

#define XR_LIST_STRUCT_XrCompositionLayerQuad(_) \
    _(type) \
    _(next) \
    _(layerFlags) \
    _(space) \
    _(eyeVisibility) \
    _(subImage) \
    _(pose) \
    _(size)

#define XR_LIST_STRUCT_XrEventDataBaseHeader(_) \
    _(type) \
    _(next)

#define XR_LIST_STRUCT_XrEventDataEventsLost(_) \
    _(type) \
    _(next) \
    _(lostEventCount)

#define XR_LIST_STRUCT_XrEventDataInstanceLossPending(_) \
    _(type) \
    _(next) \
    _(lossTime)

#define XR_LIST_STRUCT_XrEventDataSessionStateChanged(_) \
    _(type) \
    _(next) \
    _(session) \
    _(state) \
    _(time)

#define XR_LIST_STRUCT_XrEventDataReferenceSpaceChangePending(_) \
    _(type) \
    _(next) \
    _(session) \
    _(referenceSpaceType) \
    _(changeTime) \
    _(poseValid) \
    _(poseInPreviousSpace)

#define XR_LIST_STRUCT_XrEventDataInteractionProfileChanged(_) \
    _(type) \
    _(next) \
    _(session)

#define XR_LIST_STRUCT_XrHapticVibration(_) \
    _(type) \
    _(next) \
    _(duration) \
    _(frequency) \
    _(amplitude)

#define XR_LIST_STRUCT_XrOffset2Df(_) \
    _(x) \
    _(y)

#define XR_LIST_STRUCT_XrRect2Df(_) \
    _(offset) \
    _(extent)

#define XR_LIST_STRUCT_XrVector4f(_) \
    _(x) \
    _(y) \
    _(z) \
    _(w)

#define XR_LIST_STRUCT_XrColor4f(_) \
    _(r) \
    _(g) \
    _(b) \
    _(a)

#define XR_LIST_STRUCT_XrCompositionLayerCubeKHR(_) \
    _(type) \
    _(next) \
    _(layerFlags) \
    _(space) \
    _(eyeVisibility) \
    _(swapchain) \
    _(imageArrayIndex) \
    _(orientation)

#define XR_LIST_STRUCT_XrInstanceCreateInfoAndroidKHR(_) \
    _(type) \
    _(next) \
    _(applicationVM) \
    _(applicationActivity)

#define XR_LIST_STRUCT_XrCompositionLayerDepthInfoKHR(_) \
    _(type) \
    _(next) \
    _(subImage) \
    _(minDepth) \
    _(maxDepth) \
    _(nearZ) \
    _(farZ)

#define XR_LIST_STRUCT_XrVulkanSwapchainFormatListCreateInfoKHR(_) \
    _(type) \
    _(next) \
    _(viewFormatCount) \
    _(viewFormats)

#define XR_LIST_STRUCT_XrCompositionLayerCylinderKHR(_) \
    _(type) \
    _(next) \
    _(layerFlags) \
    _(space) \
    _(eyeVisibility) \
    _(subImage) \
    _(pose) \
    _(radius) \
    _(centralAngle) \
    _(aspectRatio)

#define XR_LIST_STRUCT_XrCompositionLayerEquirectKHR(_) \
    _(type) \
    _(next) \
    _(layerFlags) \
    _(space) \
    _(eyeVisibility) \
    _(subImage) \
    _(pose) \
    _(radius) \
    _(scale) \
    _(bias)

#define XR_LIST_STRUCT_XrGraphicsBindingOpenGLWin32KHR(_) \
    _(type) \
    _(next) \
    _(hDC) \
    _(hGLRC)

#define XR_LIST_STRUCT_XrGraphicsBindingOpenGLXlibKHR(_) \
    _(type) \
    _(next) \
    _(xDisplay) \
    _(visualid) \
    _(glxFBConfig) \
    _(glxDrawable) \
    _(glxContext)

#define XR_LIST_STRUCT_XrGraphicsBindingOpenGLXcbKHR(_) \
    _(type) \
    _(next) \
    _(connection) \
    _(screenNumber) \
    _(fbconfigid) \
    _(visualid) \
    _(glxDrawable) \
    _(glxContext)

#define XR_LIST_STRUCT_XrGraphicsBindingOpenGLWaylandKHR(_) \
    _(type) \
    _(next) \
    _(display)

#define XR_LIST_STRUCT_XrSwapchainImageOpenGLKHR(_) \
    _(type) \
    _(next) \
    _(image)

#define XR_LIST_STRUCT_XrGraphicsRequirementsOpenGLKHR(_) \
    _(type) \
    _(next) \
    _(minApiVersionSupported) \
    _(maxApiVersionSupported)

#define XR_LIST_STRUCT_XrGraphicsBindingOpenGLESAndroidKHR(_) \
    _(type) \
    _(next) \
    _(display) \
    _(config) \
    _(context)

#define XR_LIST_STRUCT_XrSwapchainImageOpenGLESKHR(_) \
    _(type) \
    _(next) \
    _(image)

#define XR_LIST_STRUCT_XrGraphicsRequirementsOpenGLESKHR(_) \
    _(type) \
    _(next) \
    _(minApiVersionSupported) \
    _(maxApiVersionSupported)

#define XR_LIST_STRUCT_XrGraphicsBindingVulkanKHR(_) \
    _(type) \
    _(next) \
    _(instance) \
    _(physicalDevice) \
    _(device) \
    _(queueFamilyIndex) \
    _(queueIndex)

#define XR_LIST_STRUCT_XrSwapchainImageVulkanKHR(_) \
    _(type) \
    _(next) \
    _(image)

#define XR_LIST_STRUCT_XrGraphicsRequirementsVulkanKHR(_) \
    _(type) \
    _(next) \
    _(minApiVersionSupported) \
    _(maxApiVersionSupported)

#define XR_LIST_STRUCT_XrGraphicsBindingD3D11KHR(_) \
    _(type) \
    _(next) \
    _(device)

#define XR_LIST_STRUCT_XrSwapchainImageD3D11KHR(_) \
    _(type) \
    _(next) \
    _(texture)

#define XR_LIST_STRUCT_XrGraphicsRequirementsD3D11KHR(_) \
    _(type) \
    _(next) \
    _(adapterLuid) \
    _(minFeatureLevel)

#define XR_LIST_STRUCT_XrGraphicsBindingD3D12KHR(_) \
    _(type) \
    _(next) \
    _(device) \
    _(queue)

#define XR_LIST_STRUCT_XrSwapchainImageD3D12KHR(_) \
    _(type) \
    _(next) \
    _(texture)

#define XR_LIST_STRUCT_XrGraphicsRequirementsD3D12KHR(_) \
    _(type) \
    _(next) \
    _(adapterLuid) \
    _(minFeatureLevel)

#define XR_LIST_STRUCT_XrVisibilityMaskKHR(_) \
    _(type) \
    _(next) \
    _(vertexCapacityInput) \
    _(vertexCountOutput) \
    _(vertices) \
    _(indexCapacityInput) \
    _(indexCountOutput) \
    _(indices)

#define XR_LIST_STRUCT_XrEventDataVisibilityMaskChangedKHR(_) \
    _(type) \
    _(next) \
    _(session) \
    _(viewConfigurationType) \
    _(viewIndex)

#define XR_LIST_STRUCT_XrEventDataPerfSettingsEXT(_) \
    _(type) \
    _(next) \
    _(domain) \
    _(subDomain) \
    _(fromLevel) \
    _(toLevel)

#define XR_LIST_STRUCT_XrDebugUtilsObjectNameInfoEXT(_) \
    _(type) \
    _(next) \
    _(objectType) \
    _(objectHandle) \
    _(objectName)

#define XR_LIST_STRUCT_XrDebugUtilsLabelEXT(_) \
    _(type) \
    _(next) \
    _(labelName)

#define XR_LIST_STRUCT_XrDebugUtilsMessengerCallbackDataEXT(_) \
    _(type) \
    _(next) \
    _(messageId) \
    _(functionName) \
    _(message) \
    _(objectCount) \
    _(objects) \
    _(sessionLabelCount) \
    _(sessionLabels)

#define XR_LIST_STRUCT_XrDebugUtilsMessengerCreateInfoEXT(_) \
    _(type) \
    _(next) \
    _(messageSeverities) \
    _(messageTypes) \
    _(userCallback) \
    _(userData)

#define XR_LIST_STRUCT_XrSpatialAnchorCreateInfoMSFT(_) \
    _(type) \
    _(next) \
    _(space) \
    _(pose) \
    _(time)

#define XR_LIST_STRUCT_XrSpatialAnchorSpaceCreateInfoMSFT(_) \
    _(type) \
    _(next) \
    _(anchor) \
    _(poseInAnchorSpace)




#endif


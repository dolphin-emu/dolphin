#ifndef OPENXR_H_
#define OPENXR_H_ 1

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


#ifdef __cplusplus
extern "C" {
#endif



#define XR_VERSION_1_0 1
#include "openxr_platform_defines.h"
#define XR_MAKE_VERSION(major, minor, patch) \
    ((((major) & 0xffffULL) << 48) | (((minor) & 0xffffULL) << 32) | ((patch) & 0xffffffffULL))

// OpenXR current version number.
#define XR_CURRENT_API_VERSION XR_MAKE_VERSION(1, 0, 2)

#define XR_VERSION_MAJOR(version) (uint16_t)(((uint64_t)(version) >> 48)& 0xffffULL)
#define XR_VERSION_MINOR(version) (uint16_t)(((uint64_t)(version) >> 32) & 0xffffULL)
#define XR_VERSION_PATCH(version) (uint32_t)((uint64_t)(version) & 0xffffffffULL)

#if !defined(XR_NULL_HANDLE)
#if (XR_PTR_SIZE == 8) && XR_CPP_NULLPTR_SUPPORTED
    #define XR_NULL_HANDLE nullptr
#else
    #define XR_NULL_HANDLE 0
#endif
#endif
        


#define XR_NULL_SYSTEM_ID 0


#define XR_NULL_PATH 0


#define XR_SUCCEEDED(result) ((result) >= 0)


#define XR_FAILED(result) ((result) < 0)


#define XR_UNQUALIFIED_SUCCESS(result) ((result) == 0)


#define XR_NO_DURATION 0


#define XR_INFINITE_DURATION 0x7fffffffffffffffLL


#define XR_MIN_HAPTIC_DURATION -1


#define XR_FREQUENCY_UNSPECIFIED 0


#define XR_MAX_EVENT_DATA_SIZE sizeof(XrEventDataBuffer)


#if !defined(XR_MAY_ALIAS)
#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4))
#define XR_MAY_ALIAS __attribute__((__may_alias__))
#else
#define XR_MAY_ALIAS
#endif
#endif


#if !defined(XR_DEFINE_HANDLE)
#if (XR_PTR_SIZE == 8)
    #define XR_DEFINE_HANDLE(object) typedef struct object##_T* object;
#else
    #define XR_DEFINE_HANDLE(object) typedef uint64_t object;
#endif
#endif
        


#if !defined(XR_DEFINE_ATOM)
    #define XR_DEFINE_ATOM(object) typedef uint64_t object;
#endif
        

typedef uint64_t XrVersion;
typedef uint64_t XrFlags64;
XR_DEFINE_ATOM(XrSystemId)
typedef uint32_t XrBool32;
XR_DEFINE_ATOM(XrPath)
typedef int64_t XrTime;
typedef int64_t XrDuration;
XR_DEFINE_HANDLE(XrInstance)
XR_DEFINE_HANDLE(XrSession)
XR_DEFINE_HANDLE(XrSpace)
XR_DEFINE_HANDLE(XrAction)
XR_DEFINE_HANDLE(XrSwapchain)
XR_DEFINE_HANDLE(XrActionSet)
#define XR_TRUE                           1
#define XR_FALSE                          0
#define XR_MAX_EXTENSION_NAME_SIZE        128
#define XR_MAX_API_LAYER_NAME_SIZE        256
#define XR_MAX_API_LAYER_DESCRIPTION_SIZE 256
#define XR_MAX_SYSTEM_NAME_SIZE           256
#define XR_MAX_APPLICATION_NAME_SIZE      128
#define XR_MAX_ENGINE_NAME_SIZE           128
#define XR_MAX_RUNTIME_NAME_SIZE          128
#define XR_MAX_PATH_LENGTH                256
#define XR_MAX_STRUCTURE_NAME_SIZE        64
#define XR_MAX_RESULT_STRING_SIZE         64
#define XR_MIN_COMPOSITION_LAYERS_SUPPORTED 16
#define XR_MAX_ACTION_SET_NAME_SIZE       64
#define XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE 128
#define XR_MAX_ACTION_NAME_SIZE           64
#define XR_MAX_LOCALIZED_ACTION_NAME_SIZE 128

typedef enum XrResult {
    XR_SUCCESS = 0,
    XR_TIMEOUT_EXPIRED = 1,
    XR_SESSION_LOSS_PENDING = 3,
    XR_EVENT_UNAVAILABLE = 4,
    XR_SPACE_BOUNDS_UNAVAILABLE = 7,
    XR_SESSION_NOT_FOCUSED = 8,
    XR_FRAME_DISCARDED = 9,
    XR_ERROR_VALIDATION_FAILURE = -1,
    XR_ERROR_RUNTIME_FAILURE = -2,
    XR_ERROR_OUT_OF_MEMORY = -3,
    XR_ERROR_API_VERSION_UNSUPPORTED = -4,
    XR_ERROR_INITIALIZATION_FAILED = -6,
    XR_ERROR_FUNCTION_UNSUPPORTED = -7,
    XR_ERROR_FEATURE_UNSUPPORTED = -8,
    XR_ERROR_EXTENSION_NOT_PRESENT = -9,
    XR_ERROR_LIMIT_REACHED = -10,
    XR_ERROR_SIZE_INSUFFICIENT = -11,
    XR_ERROR_HANDLE_INVALID = -12,
    XR_ERROR_INSTANCE_LOST = -13,
    XR_ERROR_SESSION_RUNNING = -14,
    XR_ERROR_SESSION_NOT_RUNNING = -16,
    XR_ERROR_SESSION_LOST = -17,
    XR_ERROR_SYSTEM_INVALID = -18,
    XR_ERROR_PATH_INVALID = -19,
    XR_ERROR_PATH_COUNT_EXCEEDED = -20,
    XR_ERROR_PATH_FORMAT_INVALID = -21,
    XR_ERROR_PATH_UNSUPPORTED = -22,
    XR_ERROR_LAYER_INVALID = -23,
    XR_ERROR_LAYER_LIMIT_EXCEEDED = -24,
    XR_ERROR_SWAPCHAIN_RECT_INVALID = -25,
    XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED = -26,
    XR_ERROR_ACTION_TYPE_MISMATCH = -27,
    XR_ERROR_SESSION_NOT_READY = -28,
    XR_ERROR_SESSION_NOT_STOPPING = -29,
    XR_ERROR_TIME_INVALID = -30,
    XR_ERROR_REFERENCE_SPACE_UNSUPPORTED = -31,
    XR_ERROR_FILE_ACCESS_ERROR = -32,
    XR_ERROR_FILE_CONTENTS_INVALID = -33,
    XR_ERROR_FORM_FACTOR_UNSUPPORTED = -34,
    XR_ERROR_FORM_FACTOR_UNAVAILABLE = -35,
    XR_ERROR_API_LAYER_NOT_PRESENT = -36,
    XR_ERROR_CALL_ORDER_INVALID = -37,
    XR_ERROR_GRAPHICS_DEVICE_INVALID = -38,
    XR_ERROR_POSE_INVALID = -39,
    XR_ERROR_INDEX_OUT_OF_RANGE = -40,
    XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED = -41,
    XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED = -42,
    XR_ERROR_NAME_DUPLICATED = -44,
    XR_ERROR_NAME_INVALID = -45,
    XR_ERROR_ACTIONSET_NOT_ATTACHED = -46,
    XR_ERROR_ACTIONSETS_ALREADY_ATTACHED = -47,
    XR_ERROR_LOCALIZED_NAME_DUPLICATED = -48,
    XR_ERROR_LOCALIZED_NAME_INVALID = -49,
    XR_ERROR_ANDROID_THREAD_SETTINGS_ID_INVALID_KHR = -1000003000,
    XR_ERROR_ANDROID_THREAD_SETTINGS_FAILURE_KHR = -1000003001,
    XR_ERROR_CREATE_SPATIAL_ANCHOR_FAILED_MSFT = -1000039001,
    XR_RESULT_MAX_ENUM = 0x7FFFFFFF
} XrResult;

typedef enum XrStructureType {
    XR_TYPE_UNKNOWN = 0,
    XR_TYPE_API_LAYER_PROPERTIES = 1,
    XR_TYPE_EXTENSION_PROPERTIES = 2,
    XR_TYPE_INSTANCE_CREATE_INFO = 3,
    XR_TYPE_SYSTEM_GET_INFO = 4,
    XR_TYPE_SYSTEM_PROPERTIES = 5,
    XR_TYPE_VIEW_LOCATE_INFO = 6,
    XR_TYPE_VIEW = 7,
    XR_TYPE_SESSION_CREATE_INFO = 8,
    XR_TYPE_SWAPCHAIN_CREATE_INFO = 9,
    XR_TYPE_SESSION_BEGIN_INFO = 10,
    XR_TYPE_VIEW_STATE = 11,
    XR_TYPE_FRAME_END_INFO = 12,
    XR_TYPE_HAPTIC_VIBRATION = 13,
    XR_TYPE_EVENT_DATA_BUFFER = 16,
    XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING = 17,
    XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED = 18,
    XR_TYPE_ACTION_STATE_BOOLEAN = 23,
    XR_TYPE_ACTION_STATE_FLOAT = 24,
    XR_TYPE_ACTION_STATE_VECTOR2F = 25,
    XR_TYPE_ACTION_STATE_POSE = 27,
    XR_TYPE_ACTION_SET_CREATE_INFO = 28,
    XR_TYPE_ACTION_CREATE_INFO = 29,
    XR_TYPE_INSTANCE_PROPERTIES = 32,
    XR_TYPE_FRAME_WAIT_INFO = 33,
    XR_TYPE_COMPOSITION_LAYER_PROJECTION = 35,
    XR_TYPE_COMPOSITION_LAYER_QUAD = 36,
    XR_TYPE_REFERENCE_SPACE_CREATE_INFO = 37,
    XR_TYPE_ACTION_SPACE_CREATE_INFO = 38,
    XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING = 40,
    XR_TYPE_VIEW_CONFIGURATION_VIEW = 41,
    XR_TYPE_SPACE_LOCATION = 42,
    XR_TYPE_SPACE_VELOCITY = 43,
    XR_TYPE_FRAME_STATE = 44,
    XR_TYPE_VIEW_CONFIGURATION_PROPERTIES = 45,
    XR_TYPE_FRAME_BEGIN_INFO = 46,
    XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW = 48,
    XR_TYPE_EVENT_DATA_EVENTS_LOST = 49,
    XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING = 51,
    XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED = 52,
    XR_TYPE_INTERACTION_PROFILE_STATE = 53,
    XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO = 55,
    XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO = 56,
    XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO = 57,
    XR_TYPE_ACTION_STATE_GET_INFO = 58,
    XR_TYPE_HAPTIC_ACTION_INFO = 59,
    XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO = 60,
    XR_TYPE_ACTIONS_SYNC_INFO = 61,
    XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO = 62,
    XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO = 63,
    XR_TYPE_COMPOSITION_LAYER_CUBE_KHR = 1000006000,
    XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR = 1000008000,
    XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR = 1000010000,
    XR_TYPE_VULKAN_SWAPCHAIN_FORMAT_LIST_CREATE_INFO_KHR = 1000014000,
    XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT = 1000015000,
    XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR = 1000017000,
    XR_TYPE_COMPOSITION_LAYER_EQUIRECT_KHR = 1000018000,
    XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT = 1000019000,
    XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT = 1000019001,
    XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT = 1000019002,
    XR_TYPE_DEBUG_UTILS_LABEL_EXT = 1000019003,
    XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR = 1000023000,
    XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR = 1000023001,
    XR_TYPE_GRAPHICS_BINDING_OPENGL_XCB_KHR = 1000023002,
    XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR = 1000023003,
    XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR = 1000023004,
    XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR = 1000023005,
    XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR = 1000024001,
    XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR = 1000024002,
    XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR = 1000024003,
    XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR = 1000025000,
    XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR = 1000025001,
    XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR = 1000025002,
    XR_TYPE_GRAPHICS_BINDING_D3D11_KHR = 1000027000,
    XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR = 1000027001,
    XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR = 1000027002,
    XR_TYPE_GRAPHICS_BINDING_D3D12_KHR = 1000028000,
    XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR = 1000028001,
    XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR = 1000028002,
    XR_TYPE_VISIBILITY_MASK_KHR = 1000031000,
    XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR = 1000031001,
    XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_MSFT = 1000039000,
    XR_TYPE_SPATIAL_ANCHOR_SPACE_CREATE_INFO_MSFT = 1000039001,
    XR_STRUCTURE_TYPE_MAX_ENUM = 0x7FFFFFFF
} XrStructureType;

typedef enum XrFormFactor {
    XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY = 1,
    XR_FORM_FACTOR_HANDHELD_DISPLAY = 2,
    XR_FORM_FACTOR_MAX_ENUM = 0x7FFFFFFF
} XrFormFactor;

typedef enum XrViewConfigurationType {
    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO = 1,
    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO = 2,
    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO = 1000037000,
    XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM = 0x7FFFFFFF
} XrViewConfigurationType;

typedef enum XrEnvironmentBlendMode {
    XR_ENVIRONMENT_BLEND_MODE_OPAQUE = 1,
    XR_ENVIRONMENT_BLEND_MODE_ADDITIVE = 2,
    XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND = 3,
    XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM = 0x7FFFFFFF
} XrEnvironmentBlendMode;

typedef enum XrReferenceSpaceType {
    XR_REFERENCE_SPACE_TYPE_VIEW = 1,
    XR_REFERENCE_SPACE_TYPE_LOCAL = 2,
    XR_REFERENCE_SPACE_TYPE_STAGE = 3,
    XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT = 1000038000,
    XR_REFERENCE_SPACE_TYPE_MAX_ENUM = 0x7FFFFFFF
} XrReferenceSpaceType;

typedef enum XrActionType {
    XR_ACTION_TYPE_BOOLEAN_INPUT = 1,
    XR_ACTION_TYPE_FLOAT_INPUT = 2,
    XR_ACTION_TYPE_VECTOR2F_INPUT = 3,
    XR_ACTION_TYPE_POSE_INPUT = 4,
    XR_ACTION_TYPE_VIBRATION_OUTPUT = 100,
    XR_ACTION_TYPE_MAX_ENUM = 0x7FFFFFFF
} XrActionType;

typedef enum XrEyeVisibility {
    XR_EYE_VISIBILITY_BOTH = 0,
    XR_EYE_VISIBILITY_LEFT = 1,
    XR_EYE_VISIBILITY_RIGHT = 2,
    XR_EYE_VISIBILITY_MAX_ENUM = 0x7FFFFFFF
} XrEyeVisibility;

typedef enum XrSessionState {
    XR_SESSION_STATE_UNKNOWN = 0,
    XR_SESSION_STATE_IDLE = 1,
    XR_SESSION_STATE_READY = 2,
    XR_SESSION_STATE_SYNCHRONIZED = 3,
    XR_SESSION_STATE_VISIBLE = 4,
    XR_SESSION_STATE_FOCUSED = 5,
    XR_SESSION_STATE_STOPPING = 6,
    XR_SESSION_STATE_LOSS_PENDING = 7,
    XR_SESSION_STATE_EXITING = 8,
    XR_SESSION_STATE_MAX_ENUM = 0x7FFFFFFF
} XrSessionState;

typedef enum XrObjectType {
    XR_OBJECT_TYPE_UNKNOWN = 0,
    XR_OBJECT_TYPE_INSTANCE = 1,
    XR_OBJECT_TYPE_SESSION = 2,
    XR_OBJECT_TYPE_SWAPCHAIN = 3,
    XR_OBJECT_TYPE_SPACE = 4,
    XR_OBJECT_TYPE_ACTION_SET = 5,
    XR_OBJECT_TYPE_ACTION = 6,
    XR_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT = 1000019000,
    XR_OBJECT_TYPE_SPATIAL_ANCHOR_MSFT = 1000039000,
    XR_OBJECT_TYPE_MAX_ENUM = 0x7FFFFFFF
} XrObjectType;
typedef XrFlags64 XrInstanceCreateFlags;

// Flag bits for XrInstanceCreateFlags

typedef XrFlags64 XrSessionCreateFlags;

// Flag bits for XrSessionCreateFlags

typedef XrFlags64 XrSpaceVelocityFlags;

// Flag bits for XrSpaceVelocityFlags
static const XrSpaceVelocityFlags XR_SPACE_VELOCITY_LINEAR_VALID_BIT = 0x00000001;
static const XrSpaceVelocityFlags XR_SPACE_VELOCITY_ANGULAR_VALID_BIT = 0x00000002;

typedef XrFlags64 XrSpaceLocationFlags;

// Flag bits for XrSpaceLocationFlags
static const XrSpaceLocationFlags XR_SPACE_LOCATION_ORIENTATION_VALID_BIT = 0x00000001;
static const XrSpaceLocationFlags XR_SPACE_LOCATION_POSITION_VALID_BIT = 0x00000002;
static const XrSpaceLocationFlags XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT = 0x00000004;
static const XrSpaceLocationFlags XR_SPACE_LOCATION_POSITION_TRACKED_BIT = 0x00000008;

typedef XrFlags64 XrSwapchainCreateFlags;

// Flag bits for XrSwapchainCreateFlags
static const XrSwapchainCreateFlags XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT = 0x00000001;
static const XrSwapchainCreateFlags XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT = 0x00000002;

typedef XrFlags64 XrSwapchainUsageFlags;

// Flag bits for XrSwapchainUsageFlags
static const XrSwapchainUsageFlags XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT = 0x00000001;
static const XrSwapchainUsageFlags XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT = 0x00000002;
static const XrSwapchainUsageFlags XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT = 0x00000004;
static const XrSwapchainUsageFlags XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT = 0x00000008;
static const XrSwapchainUsageFlags XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT = 0x00000010;
static const XrSwapchainUsageFlags XR_SWAPCHAIN_USAGE_SAMPLED_BIT = 0x00000020;
static const XrSwapchainUsageFlags XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT = 0x00000040;

typedef XrFlags64 XrCompositionLayerFlags;

// Flag bits for XrCompositionLayerFlags
static const XrCompositionLayerFlags XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT = 0x00000001;
static const XrCompositionLayerFlags XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT = 0x00000002;
static const XrCompositionLayerFlags XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT = 0x00000004;

typedef XrFlags64 XrViewStateFlags;

// Flag bits for XrViewStateFlags
static const XrViewStateFlags XR_VIEW_STATE_ORIENTATION_VALID_BIT = 0x00000001;
static const XrViewStateFlags XR_VIEW_STATE_POSITION_VALID_BIT = 0x00000002;
static const XrViewStateFlags XR_VIEW_STATE_ORIENTATION_TRACKED_BIT = 0x00000004;
static const XrViewStateFlags XR_VIEW_STATE_POSITION_TRACKED_BIT = 0x00000008;

typedef XrFlags64 XrInputSourceLocalizedNameFlags;

// Flag bits for XrInputSourceLocalizedNameFlags
static const XrInputSourceLocalizedNameFlags XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT = 0x00000001;
static const XrInputSourceLocalizedNameFlags XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT = 0x00000002;
static const XrInputSourceLocalizedNameFlags XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT = 0x00000004;

typedef void (XRAPI_PTR *PFN_xrVoidFunction)(void);
typedef struct XrApiLayerProperties {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    char                  layerName[XR_MAX_API_LAYER_NAME_SIZE];
    XrVersion             specVersion;
    uint32_t              layerVersion;
    char                  description[XR_MAX_API_LAYER_DESCRIPTION_SIZE];
} XrApiLayerProperties;

typedef struct XrExtensionProperties {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    char                  extensionName[XR_MAX_EXTENSION_NAME_SIZE];
    uint32_t              extensionVersion;
} XrExtensionProperties;

typedef struct XrApplicationInfo {
    char         applicationName[XR_MAX_APPLICATION_NAME_SIZE];
    uint32_t     applicationVersion;
    char         engineName[XR_MAX_ENGINE_NAME_SIZE];
    uint32_t     engineVersion;
    XrVersion    apiVersion;
} XrApplicationInfo;

typedef struct XrInstanceCreateInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrInstanceCreateFlags       createFlags;
    XrApplicationInfo           applicationInfo;
    uint32_t                    enabledApiLayerCount;
    const char* const*          enabledApiLayerNames;
    uint32_t                    enabledExtensionCount;
    const char* const*          enabledExtensionNames;
} XrInstanceCreateInfo;

typedef struct XrInstanceProperties {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    XrVersion             runtimeVersion;
    char                  runtimeName[XR_MAX_RUNTIME_NAME_SIZE];
} XrInstanceProperties;

typedef struct XrEventDataBuffer {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    uint8_t                     varying[4000];
} XrEventDataBuffer;

typedef struct XrSystemGetInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrFormFactor                formFactor;
} XrSystemGetInfo;

typedef struct XrSystemGraphicsProperties {
    uint32_t    maxSwapchainImageHeight;
    uint32_t    maxSwapchainImageWidth;
    uint32_t    maxLayerCount;
} XrSystemGraphicsProperties;

typedef struct XrSystemTrackingProperties {
    XrBool32    orientationTracking;
    XrBool32    positionTracking;
} XrSystemTrackingProperties;

typedef struct XrSystemProperties {
    XrStructureType               type;
    void* XR_MAY_ALIAS            next;
    XrSystemId                    systemId;
    uint32_t                      vendorId;
    char                          systemName[XR_MAX_SYSTEM_NAME_SIZE];
    XrSystemGraphicsProperties    graphicsProperties;
    XrSystemTrackingProperties    trackingProperties;
} XrSystemProperties;

typedef struct XrSessionCreateInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrSessionCreateFlags        createFlags;
    XrSystemId                  systemId;
} XrSessionCreateInfo;

typedef struct XrVector3f {
    float    x;
    float    y;
    float    z;
} XrVector3f;

typedef struct XrSpaceVelocity {
    XrStructureType         type;
    void* XR_MAY_ALIAS      next;
    XrSpaceVelocityFlags    velocityFlags;
    XrVector3f              linearVelocity;
    XrVector3f              angularVelocity;
} XrSpaceVelocity;

typedef struct XrQuaternionf {
    float    x;
    float    y;
    float    z;
    float    w;
} XrQuaternionf;

typedef struct XrPosef {
    XrQuaternionf    orientation;
    XrVector3f       position;
} XrPosef;

typedef struct XrReferenceSpaceCreateInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrReferenceSpaceType        referenceSpaceType;
    XrPosef                     poseInReferenceSpace;
} XrReferenceSpaceCreateInfo;

typedef struct XrExtent2Df {
    float    width;
    float    height;
} XrExtent2Df;

typedef struct XrActionSpaceCreateInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrAction                    action;
    XrPath                      subactionPath;
    XrPosef                     poseInActionSpace;
} XrActionSpaceCreateInfo;

typedef struct XrSpaceLocation {
    XrStructureType         type;
    void* XR_MAY_ALIAS      next;
    XrSpaceLocationFlags    locationFlags;
    XrPosef                 pose;
} XrSpaceLocation;

typedef struct XrViewConfigurationProperties {
    XrStructureType            type;
    void* XR_MAY_ALIAS         next;
    XrViewConfigurationType    viewConfigurationType;
    XrBool32                   fovMutable;
} XrViewConfigurationProperties;

typedef struct XrViewConfigurationView {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    uint32_t              recommendedImageRectWidth;
    uint32_t              maxImageRectWidth;
    uint32_t              recommendedImageRectHeight;
    uint32_t              maxImageRectHeight;
    uint32_t              recommendedSwapchainSampleCount;
    uint32_t              maxSwapchainSampleCount;
} XrViewConfigurationView;

typedef struct XrSwapchainCreateInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrSwapchainCreateFlags      createFlags;
    XrSwapchainUsageFlags       usageFlags;
    int64_t                     format;
    uint32_t                    sampleCount;
    uint32_t                    width;
    uint32_t                    height;
    uint32_t                    faceCount;
    uint32_t                    arraySize;
    uint32_t                    mipCount;
} XrSwapchainCreateInfo;

typedef struct XR_MAY_ALIAS XrSwapchainImageBaseHeader {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
} XrSwapchainImageBaseHeader;

typedef struct XrSwapchainImageAcquireInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
} XrSwapchainImageAcquireInfo;

typedef struct XrSwapchainImageWaitInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrDuration                  timeout;
} XrSwapchainImageWaitInfo;

typedef struct XrSwapchainImageReleaseInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
} XrSwapchainImageReleaseInfo;

typedef struct XrSessionBeginInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrViewConfigurationType     primaryViewConfigurationType;
} XrSessionBeginInfo;

typedef struct XrFrameWaitInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
} XrFrameWaitInfo;

typedef struct XrFrameState {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    XrTime                predictedDisplayTime;
    XrDuration            predictedDisplayPeriod;
    XrBool32              shouldRender;
} XrFrameState;

typedef struct XrFrameBeginInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
} XrFrameBeginInfo;

typedef struct XR_MAY_ALIAS XrCompositionLayerBaseHeader {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrCompositionLayerFlags     layerFlags;
    XrSpace                     space;
} XrCompositionLayerBaseHeader;

typedef struct XrFrameEndInfo {
    XrStructureType                               type;
    const void* XR_MAY_ALIAS                      next;
    XrTime                                        displayTime;
    XrEnvironmentBlendMode                        environmentBlendMode;
    uint32_t                                      layerCount;
    const XrCompositionLayerBaseHeader* const*    layers;
} XrFrameEndInfo;

typedef struct XrViewLocateInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrViewConfigurationType     viewConfigurationType;
    XrTime                      displayTime;
    XrSpace                     space;
} XrViewLocateInfo;

typedef struct XrViewState {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    XrViewStateFlags      viewStateFlags;
} XrViewState;

typedef struct XrFovf {
    float    angleLeft;
    float    angleRight;
    float    angleUp;
    float    angleDown;
} XrFovf;

typedef struct XrView {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    XrPosef               pose;
    XrFovf                fov;
} XrView;

typedef struct XrActionSetCreateInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    char                        actionSetName[XR_MAX_ACTION_SET_NAME_SIZE];
    char                        localizedActionSetName[XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE];
    uint32_t                    priority;
} XrActionSetCreateInfo;

typedef struct XrActionCreateInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    char                        actionName[XR_MAX_ACTION_NAME_SIZE];
    XrActionType                actionType;
    uint32_t                    countSubactionPaths;
    const XrPath*               subactionPaths;
    char                        localizedActionName[XR_MAX_LOCALIZED_ACTION_NAME_SIZE];
} XrActionCreateInfo;

typedef struct XrActionSuggestedBinding {
    XrAction    action;
    XrPath      binding;
} XrActionSuggestedBinding;

typedef struct XrInteractionProfileSuggestedBinding {
    XrStructureType                    type;
    const void* XR_MAY_ALIAS           next;
    XrPath                             interactionProfile;
    uint32_t                           countSuggestedBindings;
    const XrActionSuggestedBinding*    suggestedBindings;
} XrInteractionProfileSuggestedBinding;

typedef struct XrSessionActionSetsAttachInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    uint32_t                    countActionSets;
    const XrActionSet*          actionSets;
} XrSessionActionSetsAttachInfo;

typedef struct XrInteractionProfileState {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    XrPath                interactionProfile;
} XrInteractionProfileState;

typedef struct XrActionStateGetInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrAction                    action;
    XrPath                      subactionPath;
} XrActionStateGetInfo;

typedef struct XrActionStateBoolean {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    XrBool32              currentState;
    XrBool32              changedSinceLastSync;
    XrTime                lastChangeTime;
    XrBool32              isActive;
} XrActionStateBoolean;

typedef struct XrActionStateFloat {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    float                 currentState;
    XrBool32              changedSinceLastSync;
    XrTime                lastChangeTime;
    XrBool32              isActive;
} XrActionStateFloat;

typedef struct XrVector2f {
    float    x;
    float    y;
} XrVector2f;

typedef struct XrActionStateVector2f {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    XrVector2f            currentState;
    XrBool32              changedSinceLastSync;
    XrTime                lastChangeTime;
    XrBool32              isActive;
} XrActionStateVector2f;

typedef struct XrActionStatePose {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    XrBool32              isActive;
} XrActionStatePose;

typedef struct XrActiveActionSet {
    XrActionSet    actionSet;
    XrPath         subactionPath;
} XrActiveActionSet;

typedef struct XrActionsSyncInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    uint32_t                    countActiveActionSets;
    const XrActiveActionSet*    activeActionSets;
} XrActionsSyncInfo;

typedef struct XrBoundSourcesForActionEnumerateInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrAction                    action;
} XrBoundSourcesForActionEnumerateInfo;

typedef struct XrInputSourceLocalizedNameGetInfo {
    XrStructureType                    type;
    const void* XR_MAY_ALIAS           next;
    XrPath                             sourcePath;
    XrInputSourceLocalizedNameFlags    whichComponents;
} XrInputSourceLocalizedNameGetInfo;

typedef struct XrHapticActionInfo {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrAction                    action;
    XrPath                      subactionPath;
} XrHapticActionInfo;

typedef struct XR_MAY_ALIAS XrHapticBaseHeader {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
} XrHapticBaseHeader;

typedef struct XR_MAY_ALIAS XrBaseInStructure {
    XrStructureType                    type;
    const struct XrBaseInStructure*    next;
} XrBaseInStructure;

typedef struct XR_MAY_ALIAS XrBaseOutStructure {
    XrStructureType               type;
    struct XrBaseOutStructure*    next;
} XrBaseOutStructure;

typedef struct XrOffset2Di {
    int32_t    x;
    int32_t    y;
} XrOffset2Di;

typedef struct XrExtent2Di {
    int32_t    width;
    int32_t    height;
} XrExtent2Di;

typedef struct XrRect2Di {
    XrOffset2Di    offset;
    XrExtent2Di    extent;
} XrRect2Di;

typedef struct XrSwapchainSubImage {
    XrSwapchain    swapchain;
    XrRect2Di      imageRect;
    uint32_t       imageArrayIndex;
} XrSwapchainSubImage;

typedef struct XrCompositionLayerProjectionView {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrPosef                     pose;
    XrFovf                      fov;
    XrSwapchainSubImage         subImage;
} XrCompositionLayerProjectionView;

typedef struct XrCompositionLayerProjection {
    XrStructureType                            type;
    const void* XR_MAY_ALIAS                   next;
    XrCompositionLayerFlags                    layerFlags;
    XrSpace                                    space;
    uint32_t                                   viewCount;
    const XrCompositionLayerProjectionView*    views;
} XrCompositionLayerProjection;

typedef struct XrCompositionLayerQuad {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrCompositionLayerFlags     layerFlags;
    XrSpace                     space;
    XrEyeVisibility             eyeVisibility;
    XrSwapchainSubImage         subImage;
    XrPosef                     pose;
    XrExtent2Df                 size;
} XrCompositionLayerQuad;

typedef struct XR_MAY_ALIAS XrEventDataBaseHeader {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
} XrEventDataBaseHeader;

typedef struct XrEventDataEventsLost {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    uint32_t                    lostEventCount;
} XrEventDataEventsLost;

typedef struct XrEventDataInstanceLossPending {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrTime                      lossTime;
} XrEventDataInstanceLossPending;

typedef struct XrEventDataSessionStateChanged {
     XrStructureType            type;
    const void* XR_MAY_ALIAS    next;
    XrSession                   session;
    XrSessionState              state;
    XrTime                      time;
} XrEventDataSessionStateChanged;

typedef struct XrEventDataReferenceSpaceChangePending {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrSession                   session;
    XrReferenceSpaceType        referenceSpaceType;
    XrTime                      changeTime;
    XrBool32                    poseValid;
    XrPosef                     poseInPreviousSpace;
} XrEventDataReferenceSpaceChangePending;

typedef struct XrEventDataInteractionProfileChanged {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrSession                   session;
} XrEventDataInteractionProfileChanged;

typedef struct XrHapticVibration {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrDuration                  duration;
    float                       frequency;
    float                       amplitude;
} XrHapticVibration;

typedef struct XrOffset2Df {
    float    x;
    float    y;
} XrOffset2Df;

typedef struct XrRect2Df {
    XrOffset2Df    offset;
    XrExtent2Df    extent;
} XrRect2Df;

typedef struct XrVector4f {
    float    x;
    float    y;
    float    z;
    float    w;
} XrVector4f;

typedef struct XrColor4f {
    float    r;
    float    g;
    float    b;
    float    a;
} XrColor4f;

typedef XrResult (XRAPI_PTR *PFN_xrGetInstanceProcAddr)(XrInstance instance, const char* name, PFN_xrVoidFunction* function);
typedef XrResult (XRAPI_PTR *PFN_xrEnumerateApiLayerProperties)(uint32_t propertyCapacityInput, uint32_t* propertyCountOutput, XrApiLayerProperties* properties);
typedef XrResult (XRAPI_PTR *PFN_xrEnumerateInstanceExtensionProperties)(const char* layerName, uint32_t propertyCapacityInput, uint32_t* propertyCountOutput, XrExtensionProperties* properties);
typedef XrResult (XRAPI_PTR *PFN_xrCreateInstance)(const XrInstanceCreateInfo* createInfo, XrInstance* instance);
typedef XrResult (XRAPI_PTR *PFN_xrDestroyInstance)(XrInstance instance);
typedef XrResult (XRAPI_PTR *PFN_xrGetInstanceProperties)(XrInstance instance, XrInstanceProperties* instanceProperties);
typedef XrResult (XRAPI_PTR *PFN_xrPollEvent)(XrInstance instance, XrEventDataBuffer* eventData);
typedef XrResult (XRAPI_PTR *PFN_xrResultToString)(XrInstance instance, XrResult value, char buffer[XR_MAX_RESULT_STRING_SIZE]);
typedef XrResult (XRAPI_PTR *PFN_xrStructureTypeToString)(XrInstance instance, XrStructureType value, char buffer[XR_MAX_STRUCTURE_NAME_SIZE]);
typedef XrResult (XRAPI_PTR *PFN_xrGetSystem)(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId);
typedef XrResult (XRAPI_PTR *PFN_xrGetSystemProperties)(XrInstance instance, XrSystemId systemId, XrSystemProperties* properties);
typedef XrResult (XRAPI_PTR *PFN_xrEnumerateEnvironmentBlendModes)(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t environmentBlendModeCapacityInput, uint32_t* environmentBlendModeCountOutput, XrEnvironmentBlendMode* environmentBlendModes);
typedef XrResult (XRAPI_PTR *PFN_xrCreateSession)(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session);
typedef XrResult (XRAPI_PTR *PFN_xrDestroySession)(XrSession session);
typedef XrResult (XRAPI_PTR *PFN_xrEnumerateReferenceSpaces)(XrSession session, uint32_t spaceCapacityInput, uint32_t* spaceCountOutput, XrReferenceSpaceType* spaces);
typedef XrResult (XRAPI_PTR *PFN_xrCreateReferenceSpace)(XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space);
typedef XrResult (XRAPI_PTR *PFN_xrGetReferenceSpaceBoundsRect)(XrSession session, XrReferenceSpaceType referenceSpaceType, XrExtent2Df* bounds);
typedef XrResult (XRAPI_PTR *PFN_xrCreateActionSpace)(XrSession session, const XrActionSpaceCreateInfo* createInfo, XrSpace* space);
typedef XrResult (XRAPI_PTR *PFN_xrLocateSpace)(XrSpace space, XrSpace baseSpace, XrTime   time, XrSpaceLocation* location);
typedef XrResult (XRAPI_PTR *PFN_xrDestroySpace)(XrSpace space);
typedef XrResult (XRAPI_PTR *PFN_xrEnumerateViewConfigurations)(XrInstance instance, XrSystemId systemId, uint32_t viewConfigurationTypeCapacityInput, uint32_t* viewConfigurationTypeCountOutput, XrViewConfigurationType* viewConfigurationTypes);
typedef XrResult (XRAPI_PTR *PFN_xrGetViewConfigurationProperties)(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, XrViewConfigurationProperties* configurationProperties);
typedef XrResult (XRAPI_PTR *PFN_xrEnumerateViewConfigurationViews)(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrViewConfigurationView* views);
typedef XrResult (XRAPI_PTR *PFN_xrEnumerateSwapchainFormats)(XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput, int64_t* formats);
typedef XrResult (XRAPI_PTR *PFN_xrCreateSwapchain)(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain);
typedef XrResult (XRAPI_PTR *PFN_xrDestroySwapchain)(XrSwapchain swapchain);
typedef XrResult (XRAPI_PTR *PFN_xrEnumerateSwapchainImages)(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images);
typedef XrResult (XRAPI_PTR *PFN_xrAcquireSwapchainImage)(XrSwapchain         swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index);
typedef XrResult (XRAPI_PTR *PFN_xrWaitSwapchainImage)(XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo);
typedef XrResult (XRAPI_PTR *PFN_xrReleaseSwapchainImage)(XrSwapchain         swapchain, const XrSwapchainImageReleaseInfo* releaseInfo);
typedef XrResult (XRAPI_PTR *PFN_xrBeginSession)(XrSession session, const XrSessionBeginInfo* beginInfo);
typedef XrResult (XRAPI_PTR *PFN_xrEndSession)(XrSession session);
typedef XrResult (XRAPI_PTR *PFN_xrRequestExitSession)(XrSession session);
typedef XrResult (XRAPI_PTR *PFN_xrWaitFrame)(XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState);
typedef XrResult (XRAPI_PTR *PFN_xrBeginFrame)(XrSession session, const XrFrameBeginInfo* frameBeginInfo);
typedef XrResult (XRAPI_PTR *PFN_xrEndFrame)(XrSession session, const XrFrameEndInfo* frameEndInfo);
typedef XrResult (XRAPI_PTR *PFN_xrLocateViews)(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views);
typedef XrResult (XRAPI_PTR *PFN_xrStringToPath)(XrInstance instance, const char* pathString, XrPath* path);
typedef XrResult (XRAPI_PTR *PFN_xrPathToString)(XrInstance instance, XrPath path, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer);
typedef XrResult (XRAPI_PTR *PFN_xrCreateActionSet)(XrInstance instance, const XrActionSetCreateInfo* createInfo, XrActionSet* actionSet);
typedef XrResult (XRAPI_PTR *PFN_xrDestroyActionSet)(XrActionSet actionSet);
typedef XrResult (XRAPI_PTR *PFN_xrCreateAction)(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action);
typedef XrResult (XRAPI_PTR *PFN_xrDestroyAction)(XrAction action);
typedef XrResult (XRAPI_PTR *PFN_xrSuggestInteractionProfileBindings)(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings);
typedef XrResult (XRAPI_PTR *PFN_xrAttachSessionActionSets)(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo);
typedef XrResult (XRAPI_PTR *PFN_xrGetCurrentInteractionProfile)(XrSession session, XrPath topLevelUserPath, XrInteractionProfileState* interactionProfile);
typedef XrResult (XRAPI_PTR *PFN_xrGetActionStateBoolean)(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateBoolean* state);
typedef XrResult (XRAPI_PTR *PFN_xrGetActionStateFloat)(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateFloat* state);
typedef XrResult (XRAPI_PTR *PFN_xrGetActionStateVector2f)(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateVector2f* state);
typedef XrResult (XRAPI_PTR *PFN_xrGetActionStatePose)(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStatePose* state);
typedef XrResult (XRAPI_PTR *PFN_xrSyncActions)(XrSession session, const XrActionsSyncInfo* syncInfo);
typedef XrResult (XRAPI_PTR *PFN_xrEnumerateBoundSourcesForAction)(XrSession session, const XrBoundSourcesForActionEnumerateInfo* enumerateInfo, uint32_t sourceCapacityInput, uint32_t* sourceCountOutput, XrPath* sources);
typedef XrResult (XRAPI_PTR *PFN_xrGetInputSourceLocalizedName)(XrSession session, const XrInputSourceLocalizedNameGetInfo* getInfo, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer);
typedef XrResult (XRAPI_PTR *PFN_xrApplyHapticFeedback)(XrSession session, const XrHapticActionInfo* hapticActionInfo, const XrHapticBaseHeader* hapticFeedback);
typedef XrResult (XRAPI_PTR *PFN_xrStopHapticFeedback)(XrSession session, const XrHapticActionInfo* hapticActionInfo);

#ifndef XR_NO_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(
    XrInstance                                  instance,
    const char*                                 name,
    PFN_xrVoidFunction*                         function);

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateApiLayerProperties(
    uint32_t                                    propertyCapacityInput,
    uint32_t*                                   propertyCountOutput,
    XrApiLayerProperties*                       properties);

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(
    const char*                                 layerName,
    uint32_t                                    propertyCapacityInput,
    uint32_t*                                   propertyCountOutput,
    XrExtensionProperties*                      properties);

XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(
    const XrInstanceCreateInfo*                 createInfo,
    XrInstance*                                 instance);

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyInstance(
    XrInstance                                  instance);

XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProperties(
    XrInstance                                  instance,
    XrInstanceProperties*                       instanceProperties);

XRAPI_ATTR XrResult XRAPI_CALL xrPollEvent(
    XrInstance                                  instance,
    XrEventDataBuffer*                          eventData);

XRAPI_ATTR XrResult XRAPI_CALL xrResultToString(
    XrInstance                                  instance,
    XrResult                                    value,
    char                                        buffer[XR_MAX_RESULT_STRING_SIZE]);

XRAPI_ATTR XrResult XRAPI_CALL xrStructureTypeToString(
    XrInstance                                  instance,
    XrStructureType                             value,
    char                                        buffer[XR_MAX_STRUCTURE_NAME_SIZE]);

XRAPI_ATTR XrResult XRAPI_CALL xrGetSystem(
    XrInstance                                  instance,
    const XrSystemGetInfo*                      getInfo,
    XrSystemId*                                 systemId);

XRAPI_ATTR XrResult XRAPI_CALL xrGetSystemProperties(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    XrSystemProperties*                         properties);

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateEnvironmentBlendModes(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    XrViewConfigurationType                     viewConfigurationType,
    uint32_t                                    environmentBlendModeCapacityInput,
    uint32_t*                                   environmentBlendModeCountOutput,
    XrEnvironmentBlendMode*                     environmentBlendModes);

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSession(
    XrInstance                                  instance,
    const XrSessionCreateInfo*                  createInfo,
    XrSession*                                  session);

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySession(
    XrSession                                   session);

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateReferenceSpaces(
    XrSession                                   session,
    uint32_t                                    spaceCapacityInput,
    uint32_t*                                   spaceCountOutput,
    XrReferenceSpaceType*                       spaces);

XRAPI_ATTR XrResult XRAPI_CALL xrCreateReferenceSpace(
    XrSession                                   session,
    const XrReferenceSpaceCreateInfo*           createInfo,
    XrSpace*                                    space);

XRAPI_ATTR XrResult XRAPI_CALL xrGetReferenceSpaceBoundsRect(
    XrSession                                   session,
    XrReferenceSpaceType                        referenceSpaceType,
    XrExtent2Df*                                bounds);

XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSpace(
    XrSession                                   session,
    const XrActionSpaceCreateInfo*              createInfo,
    XrSpace*                                    space);

XRAPI_ATTR XrResult XRAPI_CALL xrLocateSpace(
    XrSpace                                     space,
    XrSpace                                     baseSpace,
    XrTime                                      time,
    XrSpaceLocation*                            location);

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySpace(
    XrSpace                                     space);

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateViewConfigurations(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    uint32_t                                    viewConfigurationTypeCapacityInput,
    uint32_t*                                   viewConfigurationTypeCountOutput,
    XrViewConfigurationType*                    viewConfigurationTypes);

XRAPI_ATTR XrResult XRAPI_CALL xrGetViewConfigurationProperties(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    XrViewConfigurationType                     viewConfigurationType,
    XrViewConfigurationProperties*              configurationProperties);

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateViewConfigurationViews(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    XrViewConfigurationType                     viewConfigurationType,
    uint32_t                                    viewCapacityInput,
    uint32_t*                                   viewCountOutput,
    XrViewConfigurationView*                    views);

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainFormats(
    XrSession                                   session,
    uint32_t                                    formatCapacityInput,
    uint32_t*                                   formatCountOutput,
    int64_t*                                    formats);

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSwapchain(
    XrSession                                   session,
    const XrSwapchainCreateInfo*                createInfo,
    XrSwapchain*                                swapchain);

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySwapchain(
    XrSwapchain                                 swapchain);

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainImages(
    XrSwapchain                                 swapchain,
    uint32_t                                    imageCapacityInput,
    uint32_t*                                   imageCountOutput,
    XrSwapchainImageBaseHeader*                 images);

XRAPI_ATTR XrResult XRAPI_CALL xrAcquireSwapchainImage(
    XrSwapchain                                 swapchain,
    const XrSwapchainImageAcquireInfo*          acquireInfo,
    uint32_t*                                   index);

XRAPI_ATTR XrResult XRAPI_CALL xrWaitSwapchainImage(
    XrSwapchain                                 swapchain,
    const XrSwapchainImageWaitInfo*             waitInfo);

XRAPI_ATTR XrResult XRAPI_CALL xrReleaseSwapchainImage(
    XrSwapchain                                 swapchain,
    const XrSwapchainImageReleaseInfo*          releaseInfo);

XRAPI_ATTR XrResult XRAPI_CALL xrBeginSession(
    XrSession                                   session,
    const XrSessionBeginInfo*                   beginInfo);

XRAPI_ATTR XrResult XRAPI_CALL xrEndSession(
    XrSession                                   session);

XRAPI_ATTR XrResult XRAPI_CALL xrRequestExitSession(
    XrSession                                   session);

XRAPI_ATTR XrResult XRAPI_CALL xrWaitFrame(
    XrSession                                   session,
    const XrFrameWaitInfo*                      frameWaitInfo,
    XrFrameState*                               frameState);

XRAPI_ATTR XrResult XRAPI_CALL xrBeginFrame(
    XrSession                                   session,
    const XrFrameBeginInfo*                     frameBeginInfo);

XRAPI_ATTR XrResult XRAPI_CALL xrEndFrame(
    XrSession                                   session,
    const XrFrameEndInfo*                       frameEndInfo);

XRAPI_ATTR XrResult XRAPI_CALL xrLocateViews(
    XrSession                                   session,
    const XrViewLocateInfo*                     viewLocateInfo,
    XrViewState*                                viewState,
    uint32_t                                    viewCapacityInput,
    uint32_t*                                   viewCountOutput,
    XrView*                                     views);

XRAPI_ATTR XrResult XRAPI_CALL xrStringToPath(
    XrInstance                                  instance,
    const char*                                 pathString,
    XrPath*                                     path);

XRAPI_ATTR XrResult XRAPI_CALL xrPathToString(
    XrInstance                                  instance,
    XrPath                                      path,
    uint32_t                                    bufferCapacityInput,
    uint32_t*                                   bufferCountOutput,
    char*                                       buffer);

XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSet(
    XrInstance                                  instance,
    const XrActionSetCreateInfo*                createInfo,
    XrActionSet*                                actionSet);

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyActionSet(
    XrActionSet                                 actionSet);

XRAPI_ATTR XrResult XRAPI_CALL xrCreateAction(
    XrActionSet                                 actionSet,
    const XrActionCreateInfo*                   createInfo,
    XrAction*                                   action);

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyAction(
    XrAction                                    action);

XRAPI_ATTR XrResult XRAPI_CALL xrSuggestInteractionProfileBindings(
    XrInstance                                  instance,
    const XrInteractionProfileSuggestedBinding* suggestedBindings);

XRAPI_ATTR XrResult XRAPI_CALL xrAttachSessionActionSets(
    XrSession                                   session,
    const XrSessionActionSetsAttachInfo*        attachInfo);

XRAPI_ATTR XrResult XRAPI_CALL xrGetCurrentInteractionProfile(
    XrSession                                   session,
    XrPath                                      topLevelUserPath,
    XrInteractionProfileState*                  interactionProfile);

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateBoolean(
    XrSession                                   session,
    const XrActionStateGetInfo*                 getInfo,
    XrActionStateBoolean*                       state);

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateFloat(
    XrSession                                   session,
    const XrActionStateGetInfo*                 getInfo,
    XrActionStateFloat*                         state);

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateVector2f(
    XrSession                                   session,
    const XrActionStateGetInfo*                 getInfo,
    XrActionStateVector2f*                      state);

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStatePose(
    XrSession                                   session,
    const XrActionStateGetInfo*                 getInfo,
    XrActionStatePose*                          state);

XRAPI_ATTR XrResult XRAPI_CALL xrSyncActions(
    XrSession                                   session,
    const XrActionsSyncInfo*                    syncInfo);

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateBoundSourcesForAction(
    XrSession                                   session,
    const XrBoundSourcesForActionEnumerateInfo* enumerateInfo,
    uint32_t                                    sourceCapacityInput,
    uint32_t*                                   sourceCountOutput,
    XrPath*                                     sources);

XRAPI_ATTR XrResult XRAPI_CALL xrGetInputSourceLocalizedName(
    XrSession                                   session,
    const XrInputSourceLocalizedNameGetInfo*    getInfo,
    uint32_t                                    bufferCapacityInput,
    uint32_t*                                   bufferCountOutput,
    char*                                       buffer);

XRAPI_ATTR XrResult XRAPI_CALL xrApplyHapticFeedback(
    XrSession                                   session,
    const XrHapticActionInfo*                   hapticActionInfo,
    const XrHapticBaseHeader*                   hapticFeedback);

XRAPI_ATTR XrResult XRAPI_CALL xrStopHapticFeedback(
    XrSession                                   session,
    const XrHapticActionInfo*                   hapticActionInfo);
#endif


#define XR_KHR_composition_layer_cube 1
#define XR_KHR_composition_layer_cube_SPEC_VERSION 8
#define XR_KHR_COMPOSITION_LAYER_CUBE_EXTENSION_NAME "XR_KHR_composition_layer_cube"
typedef struct XrCompositionLayerCubeKHR {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrCompositionLayerFlags     layerFlags;
    XrSpace                     space;
    XrEyeVisibility             eyeVisibility;
    XrSwapchain                 swapchain;
    uint32_t                    imageArrayIndex;
    XrQuaternionf               orientation;
} XrCompositionLayerCubeKHR;



#define XR_KHR_composition_layer_depth 1
#define XR_KHR_composition_layer_depth_SPEC_VERSION 5
#define XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME "XR_KHR_composition_layer_depth"
typedef struct XrCompositionLayerDepthInfoKHR {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrSwapchainSubImage         subImage;
    float                       minDepth;
    float                       maxDepth;
    float                       nearZ;
    float                       farZ;
} XrCompositionLayerDepthInfoKHR;



#define XR_KHR_composition_layer_cylinder 1
#define XR_KHR_composition_layer_cylinder_SPEC_VERSION 4
#define XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME "XR_KHR_composition_layer_cylinder"
typedef struct XrCompositionLayerCylinderKHR {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrCompositionLayerFlags     layerFlags;
    XrSpace                     space;
    XrEyeVisibility             eyeVisibility;
    XrSwapchainSubImage         subImage;
    XrPosef                     pose;
    float                       radius;
    float                       centralAngle;
    float                       aspectRatio;
} XrCompositionLayerCylinderKHR;



#define XR_KHR_composition_layer_equirect 1
#define XR_KHR_composition_layer_equirect_SPEC_VERSION 3
#define XR_KHR_COMPOSITION_LAYER_EQUIRECT_EXTENSION_NAME "XR_KHR_composition_layer_equirect"
typedef struct XrCompositionLayerEquirectKHR {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrCompositionLayerFlags     layerFlags;
    XrSpace                     space;
    XrEyeVisibility             eyeVisibility;
    XrSwapchainSubImage         subImage;
    XrPosef                     pose;
    float                       radius;
    XrVector2f                  scale;
    XrVector2f                  bias;
} XrCompositionLayerEquirectKHR;



#define XR_KHR_visibility_mask 1
#define XR_KHR_visibility_mask_SPEC_VERSION 2
#define XR_KHR_VISIBILITY_MASK_EXTENSION_NAME "XR_KHR_visibility_mask"

typedef enum XrVisibilityMaskTypeKHR {
    XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR = 1,
    XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR = 2,
    XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR = 3,
    XR_VISIBILITY_MASK_TYPE_MAX_ENUM_KHR = 0x7FFFFFFF
} XrVisibilityMaskTypeKHR;
typedef struct XrVisibilityMaskKHR {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    uint32_t              vertexCapacityInput;
    uint32_t              vertexCountOutput;
    XrVector2f*           vertices;
    uint32_t              indexCapacityInput;
    uint32_t              indexCountOutput;
    uint32_t*             indices;
} XrVisibilityMaskKHR;

typedef struct XrEventDataVisibilityMaskChangedKHR {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrSession                   session;
    XrViewConfigurationType     viewConfigurationType;
    uint32_t                    viewIndex;
} XrEventDataVisibilityMaskChangedKHR;

typedef XrResult (XRAPI_PTR *PFN_xrGetVisibilityMaskKHR)(XrSession session, XrViewConfigurationType viewConfigurationType, uint32_t viewIndex, XrVisibilityMaskTypeKHR visibilityMaskType, XrVisibilityMaskKHR* visibilityMask);

#ifndef XR_NO_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrGetVisibilityMaskKHR(
    XrSession                                   session,
    XrViewConfigurationType                     viewConfigurationType,
    uint32_t                                    viewIndex,
    XrVisibilityMaskTypeKHR                     visibilityMaskType,
    XrVisibilityMaskKHR*                        visibilityMask);
#endif


#define XR_EXT_performance_settings 1
#define XR_EXT_performance_settings_SPEC_VERSION 1
#define XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME "XR_EXT_performance_settings"

typedef enum XrPerfSettingsDomainEXT {
    XR_PERF_SETTINGS_DOMAIN_CPU_EXT = 1,
    XR_PERF_SETTINGS_DOMAIN_GPU_EXT = 2,
    XR_PERF_SETTINGS_DOMAIN_MAX_ENUM_EXT = 0x7FFFFFFF
} XrPerfSettingsDomainEXT;

typedef enum XrPerfSettingsSubDomainEXT {
    XR_PERF_SETTINGS_SUB_DOMAIN_COMPOSITING_EXT = 1,
    XR_PERF_SETTINGS_SUB_DOMAIN_RENDERING_EXT = 2,
    XR_PERF_SETTINGS_SUB_DOMAIN_THERMAL_EXT = 3,
    XR_PERF_SETTINGS_SUB_DOMAIN_MAX_ENUM_EXT = 0x7FFFFFFF
} XrPerfSettingsSubDomainEXT;

typedef enum XrPerfSettingsLevelEXT {
    XR_PERF_SETTINGS_LEVEL_POWER_SAVINGS_EXT = 0,
    XR_PERF_SETTINGS_LEVEL_SUSTAINED_LOW_EXT = 25,
    XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT = 50,
    XR_PERF_SETTINGS_LEVEL_BOOST_EXT = 75,
    XR_PERF_SETTINGS_LEVEL_MAX_ENUM_EXT = 0x7FFFFFFF
} XrPerfSettingsLevelEXT;

typedef enum XrPerfSettingsNotificationLevelEXT {
    XR_PERF_SETTINGS_NOTIF_LEVEL_NORMAL_EXT = 0,
    XR_PERF_SETTINGS_NOTIF_LEVEL_WARNING_EXT = 25,
    XR_PERF_SETTINGS_NOTIF_LEVEL_IMPAIRED_EXT = 75,
    XR_PERF_SETTINGS_NOTIFICATION_LEVEL_MAX_ENUM_EXT = 0x7FFFFFFF
} XrPerfSettingsNotificationLevelEXT;
typedef struct XrEventDataPerfSettingsEXT {
    XrStructureType                       type;
    const void* XR_MAY_ALIAS              next;
    XrPerfSettingsDomainEXT               domain;
    XrPerfSettingsSubDomainEXT            subDomain;
    XrPerfSettingsNotificationLevelEXT    fromLevel;
    XrPerfSettingsNotificationLevelEXT    toLevel;
} XrEventDataPerfSettingsEXT;

typedef XrResult (XRAPI_PTR *PFN_xrPerfSettingsSetPerformanceLevelEXT)(XrSession session, XrPerfSettingsDomainEXT domain, XrPerfSettingsLevelEXT level);

#ifndef XR_NO_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrPerfSettingsSetPerformanceLevelEXT(
    XrSession                                   session,
    XrPerfSettingsDomainEXT                     domain,
    XrPerfSettingsLevelEXT                      level);
#endif


#define XR_EXT_thermal_query 1
#define XR_EXT_thermal_query_SPEC_VERSION 1
#define XR_EXT_THERMAL_QUERY_EXTENSION_NAME "XR_EXT_thermal_query"
typedef XrResult (XRAPI_PTR *PFN_xrThermalGetTemperatureTrendEXT)(XrSession session, XrPerfSettingsDomainEXT domain, XrPerfSettingsNotificationLevelEXT* notificationLevel, float* tempHeadroom, float* tempSlope);

#ifndef XR_NO_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrThermalGetTemperatureTrendEXT(
    XrSession                                   session,
    XrPerfSettingsDomainEXT                     domain,
    XrPerfSettingsNotificationLevelEXT*         notificationLevel,
    float*                                      tempHeadroom,
    float*                                      tempSlope);
#endif


#define XR_EXT_debug_utils 1
XR_DEFINE_HANDLE(XrDebugUtilsMessengerEXT)
#define XR_EXT_debug_utils_SPEC_VERSION   3
#define XR_EXT_DEBUG_UTILS_EXTENSION_NAME "XR_EXT_debug_utils"
typedef XrFlags64 XrDebugUtilsMessageSeverityFlagsEXT;

// Flag bits for XrDebugUtilsMessageSeverityFlagsEXT
static const XrDebugUtilsMessageSeverityFlagsEXT XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT = 0x00000001;
static const XrDebugUtilsMessageSeverityFlagsEXT XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT = 0x00000010;
static const XrDebugUtilsMessageSeverityFlagsEXT XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT = 0x00000100;
static const XrDebugUtilsMessageSeverityFlagsEXT XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT = 0x00001000;

typedef XrFlags64 XrDebugUtilsMessageTypeFlagsEXT;

// Flag bits for XrDebugUtilsMessageTypeFlagsEXT
static const XrDebugUtilsMessageTypeFlagsEXT XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT = 0x00000001;
static const XrDebugUtilsMessageTypeFlagsEXT XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT = 0x00000002;
static const XrDebugUtilsMessageTypeFlagsEXT XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT = 0x00000004;
static const XrDebugUtilsMessageTypeFlagsEXT XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT = 0x00000008;

typedef struct XrDebugUtilsObjectNameInfoEXT {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrObjectType                objectType;
    uint64_t                    objectHandle;
    const char*                 objectName;
} XrDebugUtilsObjectNameInfoEXT;

typedef struct XrDebugUtilsLabelEXT {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    const char*                 labelName;
} XrDebugUtilsLabelEXT;

typedef struct XrDebugUtilsMessengerCallbackDataEXT {
    XrStructureType                   type;
    const void* XR_MAY_ALIAS          next;
    const char*                       messageId;
    const char*                       functionName;
    const char*                       message;
    uint32_t                          objectCount;
    XrDebugUtilsObjectNameInfoEXT*    objects;
    uint32_t                          sessionLabelCount;
    XrDebugUtilsLabelEXT*             sessionLabels;
} XrDebugUtilsMessengerCallbackDataEXT;

typedef XrBool32 (XRAPI_PTR *PFN_xrDebugUtilsMessengerCallbackEXT)(
            XrDebugUtilsMessageSeverityFlagsEXT              messageSeverity,
            XrDebugUtilsMessageTypeFlagsEXT                  messageTypes,
            const XrDebugUtilsMessengerCallbackDataEXT*      callbackData,
            void*                                            userData);
        

typedef struct XrDebugUtilsMessengerCreateInfoEXT {
    XrStructureType                         type;
    const void* XR_MAY_ALIAS                next;
    XrDebugUtilsMessageSeverityFlagsEXT     messageSeverities;
    XrDebugUtilsMessageTypeFlagsEXT         messageTypes;
    PFN_xrDebugUtilsMessengerCallbackEXT    userCallback;
    void* XR_MAY_ALIAS                      userData;
} XrDebugUtilsMessengerCreateInfoEXT;

typedef XrResult (XRAPI_PTR *PFN_xrSetDebugUtilsObjectNameEXT)(XrInstance instance, const XrDebugUtilsObjectNameInfoEXT* nameInfo);
typedef XrResult (XRAPI_PTR *PFN_xrCreateDebugUtilsMessengerEXT)(XrInstance instance, const XrDebugUtilsMessengerCreateInfoEXT* createInfo, XrDebugUtilsMessengerEXT* messenger);
typedef XrResult (XRAPI_PTR *PFN_xrDestroyDebugUtilsMessengerEXT)(XrDebugUtilsMessengerEXT messenger);
typedef XrResult                                    (XRAPI_PTR *PFN_xrSubmitDebugUtilsMessageEXT)(XrInstance                                  instance, XrDebugUtilsMessageSeverityFlagsEXT         messageSeverity, XrDebugUtilsMessageTypeFlagsEXT             messageTypes, const XrDebugUtilsMessengerCallbackDataEXT* callbackData);
typedef XrResult (XRAPI_PTR *PFN_xrSessionBeginDebugUtilsLabelRegionEXT)(XrSession session, const XrDebugUtilsLabelEXT* labelInfo);
typedef XrResult (XRAPI_PTR *PFN_xrSessionEndDebugUtilsLabelRegionEXT)(XrSession session);
typedef XrResult (XRAPI_PTR *PFN_xrSessionInsertDebugUtilsLabelEXT)(XrSession session, const XrDebugUtilsLabelEXT* labelInfo);

#ifndef XR_NO_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrSetDebugUtilsObjectNameEXT(
    XrInstance                                  instance,
    const XrDebugUtilsObjectNameInfoEXT*        nameInfo);

XRAPI_ATTR XrResult XRAPI_CALL xrCreateDebugUtilsMessengerEXT(
    XrInstance                                  instance,
    const XrDebugUtilsMessengerCreateInfoEXT*   createInfo,
    XrDebugUtilsMessengerEXT*                   messenger);

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyDebugUtilsMessengerEXT(
    XrDebugUtilsMessengerEXT                    messenger);

XRAPI_ATTR XrResult                                    XRAPI_CALL xrSubmitDebugUtilsMessageEXT(
    XrInstance                                  instance,
    XrDebugUtilsMessageSeverityFlagsEXT         messageSeverity,
    XrDebugUtilsMessageTypeFlagsEXT             messageTypes,
    const XrDebugUtilsMessengerCallbackDataEXT* callbackData);

XRAPI_ATTR XrResult XRAPI_CALL xrSessionBeginDebugUtilsLabelRegionEXT(
    XrSession                                   session,
    const XrDebugUtilsLabelEXT*                 labelInfo);

XRAPI_ATTR XrResult XRAPI_CALL xrSessionEndDebugUtilsLabelRegionEXT(
    XrSession                                   session);

XRAPI_ATTR XrResult XRAPI_CALL xrSessionInsertDebugUtilsLabelEXT(
    XrSession                                   session,
    const XrDebugUtilsLabelEXT*                 labelInfo);
#endif


#define XR_VARJO_quad_views 1
#define XR_VARJO_quad_views_SPEC_VERSION  1
#define XR_VARJO_QUAD_VIEWS_EXTENSION_NAME "XR_VARJO_quad_views"


#define XR_MSFT_unbounded_reference_space 1
#define XR_MSFT_unbounded_reference_space_SPEC_VERSION 1
#define XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME "XR_MSFT_unbounded_reference_space"


#define XR_MSFT_spatial_anchor 1
XR_DEFINE_HANDLE(XrSpatialAnchorMSFT)
#define XR_MSFT_spatial_anchor_SPEC_VERSION 1
#define XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME "XR_MSFT_spatial_anchor"
typedef struct XrSpatialAnchorCreateInfoMSFT {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrSpace                     space;
    XrPosef                     pose;
    XrTime                      time;
} XrSpatialAnchorCreateInfoMSFT;

typedef struct XrSpatialAnchorSpaceCreateInfoMSFT {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrSpatialAnchorMSFT         anchor;
    XrPosef                     poseInAnchorSpace;
} XrSpatialAnchorSpaceCreateInfoMSFT;

typedef XrResult (XRAPI_PTR *PFN_xrCreateSpatialAnchorMSFT)(XrSession session, const XrSpatialAnchorCreateInfoMSFT* createInfo, XrSpatialAnchorMSFT* anchor);
typedef XrResult (XRAPI_PTR *PFN_xrCreateSpatialAnchorSpaceMSFT)(XrSession session, const XrSpatialAnchorSpaceCreateInfoMSFT* createInfo, XrSpace* space);
typedef XrResult (XRAPI_PTR *PFN_xrDestroySpatialAnchorMSFT)(XrSpatialAnchorMSFT anchor);

#ifndef XR_NO_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrCreateSpatialAnchorMSFT(
    XrSession                                   session,
    const XrSpatialAnchorCreateInfoMSFT*        createInfo,
    XrSpatialAnchorMSFT*                        anchor);

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSpatialAnchorSpaceMSFT(
    XrSession                                   session,
    const XrSpatialAnchorSpaceCreateInfoMSFT*   createInfo,
    XrSpace*                                    space);

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySpatialAnchorMSFT(
    XrSpatialAnchorMSFT                         anchor);
#endif


#define XR_MND_headless 1
#define XR_MND_headless_SPEC_VERSION      1
#define XR_MND_HEADLESS_EXTENSION_NAME    "XR_MND_headless"


#define XR_OCULUS_android_session_state_enable 1
#define XR_OCULUS_android_session_state_enable_SPEC_VERSION 1
#define XR_OCULUS_ANDROID_SESSION_STATE_ENABLE_EXTENSION_NAME "XR_OCULUS_android_session_state_enable"

#ifdef __cplusplus
}
#endif

#endif

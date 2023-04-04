#define _USE_MATH_DEFINES
#include <cmath>

#include "VRBase.h"
#include "VRInput.h"
#include "VRRenderer.h"
#include "OpenXRLoader.h"

#include <cstdlib>
#include <cstring>

XrFovf fov;
XrView* projections;
XrPosef invViewTransform[2];
XrFrameState frameState = {};
bool initialized = false;
bool stageSupported = false;
int vrConfig[VR_CONFIG_MAX] = {};
float vrConfigFloat[VR_CONFIG_FLOAT_MAX] = {};

XrVector3f hmdorientation;

void VR_UpdateStageBounds(ovrApp* pappState) {
	XrExtent2Df stageBounds = {};

	XrResult result;
	OXR(result = xrGetReferenceSpaceBoundsRect(pappState->Session, XR_REFERENCE_SPACE_TYPE_STAGE, &stageBounds));
	if (result != XR_SUCCESS) {
		ALOGV("Stage bounds query failed: using small defaults");
		stageBounds.width = 1.0f;
		stageBounds.height = 1.0f;

		pappState->CurrentSpace = pappState->FakeStageSpace;
	}

	ALOGV("Stage bounds: width = %f, depth %f", stageBounds.width, stageBounds.height);
}

void VR_GetResolution(engine_t* engine, int *pWidth, int *pHeight) {
	static int width = 0;
	static int height = 0;

	if (engine) {
		// Enumerate the viewport configurations.
		uint32_t viewportConfigTypeCount = 0;
		OXR(xrEnumerateViewConfigurations(
				engine->appState.Instance, engine->appState.SystemId, 0, &viewportConfigTypeCount, NULL));

		XrViewConfigurationType* viewportConfigurationTypes =
				(XrViewConfigurationType*)malloc(viewportConfigTypeCount * sizeof(XrViewConfigurationType));

		OXR(xrEnumerateViewConfigurations(
				engine->appState.Instance,
				engine->appState.SystemId,
				viewportConfigTypeCount,
				&viewportConfigTypeCount,
				viewportConfigurationTypes));

		ALOGV("Available Viewport Configuration Types: %d", viewportConfigTypeCount);

		for (uint32_t i = 0; i < viewportConfigTypeCount; i++) {
			const XrViewConfigurationType viewportConfigType = viewportConfigurationTypes[i];

			ALOGV(
					"Viewport configuration type %d : %s",
					viewportConfigType,
					viewportConfigType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO ? "Selected" : "");

			XrViewConfigurationProperties viewportConfig;
			viewportConfig.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;
			OXR(xrGetViewConfigurationProperties(
					engine->appState.Instance, engine->appState.SystemId, viewportConfigType, &viewportConfig));
			ALOGV(
					"FovMutable=%s ConfigurationType %d",
					viewportConfig.fovMutable ? "true" : "false",
					viewportConfig.viewConfigurationType);

			uint32_t viewCount;
			OXR(xrEnumerateViewConfigurationViews(
					engine->appState.Instance, engine->appState.SystemId, viewportConfigType, 0, &viewCount, NULL));

			if (viewCount > 0) {
				XrViewConfigurationView* elements =
						(XrViewConfigurationView*)malloc(viewCount * sizeof(XrViewConfigurationView));

				for (uint32_t e = 0; e < viewCount; e++) {
					elements[e].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
					elements[e].next = NULL;
				}

				OXR(xrEnumerateViewConfigurationViews(
						engine->appState.Instance,
						engine->appState.SystemId,
						viewportConfigType,
						viewCount,
						&viewCount,
						elements));

				// Cache the view config properties for the selected config type.
				if (viewportConfigType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
					assert(viewCount == ovrMaxNumEyes);
					for (uint32_t e = 0; e < viewCount; e++) {
						engine->appState.ViewConfigurationView[e] = elements[e];
					}
				}

				free(elements);
			} else {
				ALOGE("Empty viewport configuration type: %d", viewCount);
			}
		}

		free(viewportConfigurationTypes);

		*pWidth = width = engine->appState.ViewConfigurationView[0].recommendedImageRectWidth;
		*pHeight = height = engine->appState.ViewConfigurationView[0].recommendedImageRectHeight;
	} else {
		//use cached values
		*pWidth = width;
		*pHeight = height;
	}
}

void VR_Recenter(engine_t* engine) {

	// Calculate recenter reference
	XrReferenceSpaceCreateInfo spaceCreateInfo = {};
	spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
	if (engine->appState.CurrentSpace != XR_NULL_HANDLE) {
		XrSpaceLocation loc = {};
		loc.type = XR_TYPE_SPACE_LOCATION;
		OXR(xrLocateSpace(engine->appState.HeadSpace, engine->appState.CurrentSpace, engine->predictedDisplayTime, &loc));
		hmdorientation = XrQuaternionf_ToEulerAngles(loc.pose.orientation);

		VR_SetConfigFloat(VR_CONFIG_RECENTER_YAW, VR_GetConfigFloat(VR_CONFIG_RECENTER_YAW) + hmdorientation.y);
		float recenterYaw = ToRadians(VR_GetConfigFloat(VR_CONFIG_RECENTER_YAW));
		spaceCreateInfo.poseInReferenceSpace.orientation.x = 0;
		spaceCreateInfo.poseInReferenceSpace.orientation.y = sinf(recenterYaw / 2);
		spaceCreateInfo.poseInReferenceSpace.orientation.z = 0;
		spaceCreateInfo.poseInReferenceSpace.orientation.w = cosf(recenterYaw / 2);
	}

	// Delete previous space instances
	if (engine->appState.StageSpace != XR_NULL_HANDLE) {
		OXR(xrDestroySpace(engine->appState.StageSpace));
	}
	if (engine->appState.FakeStageSpace != XR_NULL_HANDLE) {
		OXR(xrDestroySpace(engine->appState.FakeStageSpace));
	}

	// Create a default stage space to use if SPACE_TYPE_STAGE is not
	// supported, or calls to xrGetReferenceSpaceBoundsRect fail.
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	if (VR_GetPlatformFlag(VR_PLATFORM_TRACKING_FLOOR)) {
		spaceCreateInfo.poseInReferenceSpace.position.y = -1.6750f;
	}
	OXR(xrCreateReferenceSpace(engine->appState.Session, &spaceCreateInfo, &engine->appState.FakeStageSpace));
	ALOGV("Created fake stage space from local space with offset");
	engine->appState.CurrentSpace = engine->appState.FakeStageSpace;

	if (stageSupported) {
		spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
		spaceCreateInfo.poseInReferenceSpace.position.y = 0.0;
		OXR(xrCreateReferenceSpace(engine->appState.Session, &spaceCreateInfo, &engine->appState.StageSpace));
		ALOGV("Created stage space");
		if (VR_GetPlatformFlag(VR_PLATFORM_TRACKING_FLOOR)) {
			engine->appState.CurrentSpace = engine->appState.StageSpace;
		}
	}

	// Update menu orientation
	VR_SetConfigFloat(VR_CONFIG_MENU_PITCH, hmdorientation.x);
	VR_SetConfigFloat(VR_CONFIG_MENU_YAW, 0.0f);
}

void VR_InitRenderer( engine_t* engine, bool multiview ) {
	if (initialized) {
		VR_DestroyRenderer(engine);
	}

	int eyeW, eyeH;
	VR_GetResolution(engine, &eyeW, &eyeH);
	VR_SetConfig(VR_CONFIG_VIEWPORT_WIDTH, eyeW);
	VR_SetConfig(VR_CONFIG_VIEWPORT_HEIGHT, eyeH);

	// Get the viewport configuration info for the chosen viewport configuration type.
	engine->appState.ViewportConfig.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;
	OXR(xrGetViewConfigurationProperties(engine->appState.Instance, engine->appState.SystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, &engine->appState.ViewportConfig));

	uint32_t numOutputSpaces = 0;
	OXR(xrEnumerateReferenceSpaces(engine->appState.Session, 0, &numOutputSpaces, NULL));
	XrReferenceSpaceType* referenceSpaces = (XrReferenceSpaceType*)malloc(numOutputSpaces * sizeof(XrReferenceSpaceType));
	OXR(xrEnumerateReferenceSpaces(engine->appState.Session, numOutputSpaces, &numOutputSpaces, referenceSpaces));

	for (uint32_t i = 0; i < numOutputSpaces; i++) {
		if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_STAGE) {
			stageSupported = true;
			break;
		}
	}

	free(referenceSpaces);

	if (engine->appState.CurrentSpace == XR_NULL_HANDLE) {
		VR_Recenter(engine);
	}

	projections = (XrView*)(malloc(ovrMaxNumEyes * sizeof(XrView)));

	void* vulkanContext = nullptr;
	if (VR_GetPlatformFlag(VR_PLATFORM_RENDERER_VULKAN)) {
		vulkanContext = &engine->graphicsBindingVulkan;
	}
	ovrRenderer_Create(engine->appState.Session, &engine->appState.Renderer,
			engine->appState.ViewConfigurationView[0].recommendedImageRectWidth,
			engine->appState.ViewConfigurationView[0].recommendedImageRectHeight,
			multiview, vulkanContext);
#ifdef ANDROID
	if (VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_FOVEATION)) {
		ovrRenderer_SetFoveation(&engine->appState.Instance, &engine->appState.Session, &engine->appState.Renderer, XR_FOVEATION_LEVEL_HIGH_TOP_FB, 0, XR_FOVEATION_DYNAMIC_LEVEL_ENABLED_FB);
	}
#endif
	initialized = true;
}

void VR_DestroyRenderer( engine_t* engine ) {
	ovrRenderer_Destroy(&engine->appState.Renderer);
	free(projections);
	initialized = false;
}

bool VR_InitFrame( engine_t* engine ) {
	bool stageBoundsDirty = true;
	if (ovrApp_HandleXrEvents(&engine->appState)) {
		VR_Recenter(engine);
	}
	if (engine->appState.SessionActive == false) {
		return false;
	}

	if (stageBoundsDirty) {
		VR_UpdateStageBounds(&engine->appState);
		stageBoundsDirty = false;
	}

	// NOTE: OpenXR does not use the concept of frame indices. Instead,
	// XrWaitFrame returns the predicted display time.
	XrFrameWaitInfo waitFrameInfo = {};
	waitFrameInfo.type = XR_TYPE_FRAME_WAIT_INFO;
	waitFrameInfo.next = NULL;

	frameState.type = XR_TYPE_FRAME_STATE;
	frameState.next = NULL;

	OXR(xrWaitFrame(engine->appState.Session, &waitFrameInfo, &frameState));
	engine->predictedDisplayTime = frameState.predictedDisplayTime;

	// Get the HMD pose, predicted for the middle of the time period during which
	// the new eye images will be displayed. The number of frames predicted ahead
	// depends on the pipeline depth of the engine and the synthesis rate.
	// The better the prediction, the less black will be pulled in at the edges.
	XrFrameBeginInfo beginFrameDesc = {};
	beginFrameDesc.type = XR_TYPE_FRAME_BEGIN_INFO;
	beginFrameDesc.next = NULL;
	OXR(xrBeginFrame(engine->appState.Session, &beginFrameDesc));

	XrViewLocateInfo projectionInfo = {};
	projectionInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
	projectionInfo.viewConfigurationType = engine->appState.ViewportConfig.viewConfigurationType;
	projectionInfo.displayTime = frameState.predictedDisplayTime;
	projectionInfo.space = engine->appState.CurrentSpace;

	XrViewState viewState = {XR_TYPE_VIEW_STATE, NULL};

	uint32_t projectionCapacityInput = ovrMaxNumEyes;
	uint32_t projectionCountOutput = projectionCapacityInput;

	OXR(xrLocateViews(
			engine->appState.Session,
			&projectionInfo,
			&viewState,
			projectionCapacityInput,
			&projectionCountOutput,
			projections));
	//

	fov = {};
	for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
		fov.angleLeft += projections[eye].fov.angleLeft / 2.0f;
		fov.angleRight += projections[eye].fov.angleRight / 2.0f;
		fov.angleUp += projections[eye].fov.angleUp / 2.0f;
		fov.angleDown += projections[eye].fov.angleDown / 2.0f;
		invViewTransform[eye] = projections[eye].pose;
	}

	// Update HMD and controllers
	hmdorientation = XrQuaternionf_ToEulerAngles(invViewTransform[0].orientation);
	IN_VRInputFrame(engine);

	engine->appState.LayerCount = 0;
	memset(engine->appState.Layers, 0, sizeof(ovrCompositorLayer_Union) * ovrMaxLayerCount);
	return true;
}

void VR_BeginFrame( engine_t* engine, int fboIndex ) {
	vrConfig[VR_CONFIG_CURRENT_FBO] = fboIndex;
	ovrFramebuffer_Acquire(&engine->appState.Renderer.FrameBuffer[fboIndex]);
}

void VR_EndFrame( engine_t* engine ) {
	int fboIndex = vrConfig[VR_CONFIG_CURRENT_FBO];
	VR_BindFramebuffer(engine);

	// Show mouse cursor
	int vrMode = vrConfig[VR_CONFIG_MODE];
	bool screenMode = (vrMode == VR_MODE_MONO_SCREEN) || (vrMode == VR_MODE_STEREO_SCREEN);
	if (screenMode && (vrConfig[VR_CONFIG_MOUSE_SIZE] > 0)) {
		int x = vrConfig[VR_CONFIG_MOUSE_X];
		int y = vrConfig[VR_CONFIG_MOUSE_Y];
		int sx = vrConfig[VR_CONFIG_MOUSE_SIZE];
		int sy = (int)((float)sx * VR_GetConfigFloat(VR_CONFIG_CANVAS_ASPECT));
		ovrRenderer_MouseCursor(&engine->appState.Renderer, x, y, sx, sy);
	}

	ovrFramebuffer_Release(&engine->appState.Renderer.FrameBuffer[fboIndex]);
}

void VR_FinishFrame( engine_t* engine ) {

	int vrMode = vrConfig[VR_CONFIG_MODE];
	XrCompositionLayerProjectionView projection_layer_elements[2] = {};
	if ((vrMode == VR_MODE_MONO_6DOF) || (vrMode == VR_MODE_STEREO_6DOF)) {
		VR_SetConfigFloat(VR_CONFIG_MENU_YAW, hmdorientation.y);

		for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
			int imageLayer = engine->appState.Renderer.Multiview ? eye : 0;
			ovrFramebuffer* frameBuffer = &engine->appState.Renderer.FrameBuffer[0];
			XrPosef pose = invViewTransform[0];
			if (vrMode != VR_MODE_MONO_6DOF) {
				if (!engine->appState.Renderer.Multiview) {
					frameBuffer = &engine->appState.Renderer.FrameBuffer[eye];
				}
				pose = invViewTransform[eye];
			}

			memset(&projection_layer_elements[eye], 0, sizeof(XrCompositionLayerProjectionView));
			projection_layer_elements[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
			projection_layer_elements[eye].pose = pose;
			projection_layer_elements[eye].fov = fov;

			memset(&projection_layer_elements[eye].subImage, 0, sizeof(XrSwapchainSubImage));
			projection_layer_elements[eye].subImage.swapchain = frameBuffer->ColorSwapChain.Handle;
			projection_layer_elements[eye].subImage.imageRect.offset.x = 0;
			projection_layer_elements[eye].subImage.imageRect.offset.y = 0;
			projection_layer_elements[eye].subImage.imageRect.extent.width = frameBuffer->ColorSwapChain.Width;
			projection_layer_elements[eye].subImage.imageRect.extent.height = frameBuffer->ColorSwapChain.Height;
			projection_layer_elements[eye].subImage.imageArrayIndex = imageLayer;
		}

		XrCompositionLayerProjection projection_layer = {};
		projection_layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
		projection_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		projection_layer.layerFlags |= XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
		projection_layer.space = engine->appState.CurrentSpace;
		projection_layer.viewCount = ovrMaxNumEyes;
		projection_layer.views = projection_layer_elements;

		engine->appState.Layers[engine->appState.LayerCount++].Projection = projection_layer;
	} else if ((vrMode == VR_MODE_MONO_SCREEN) || (vrMode == VR_MODE_STEREO_SCREEN)) {

		// Flat screen pose
		float distance = VR_GetConfigFloat(VR_CONFIG_CANVAS_DISTANCE);
		float menuPitch = ToRadians(VR_GetConfigFloat(VR_CONFIG_MENU_PITCH));
		float menuYaw = ToRadians(VR_GetConfigFloat(VR_CONFIG_MENU_YAW));
		XrVector3f pos = {
				invViewTransform[0].position.x - sinf(menuYaw) * distance,
				invViewTransform[0].position.y,
				invViewTransform[0].position.z - cosf(menuYaw) * distance
		};
		XrQuaternionf pitch = XrQuaternionf_CreateFromVectorAngle({1, 0, 0}, -menuPitch);
		XrQuaternionf yaw = XrQuaternionf_CreateFromVectorAngle({0, 1, 0}, menuYaw);

		// Setup the cylinder layer
		XrCompositionLayerCylinderKHR cylinder_layer = {};
		cylinder_layer.type = XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR;
		cylinder_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		cylinder_layer.space = engine->appState.CurrentSpace;
		memset(&cylinder_layer.subImage, 0, sizeof(XrSwapchainSubImage));
		cylinder_layer.subImage.imageRect.offset.x = 0;
		cylinder_layer.subImage.imageRect.offset.y = 0;
		cylinder_layer.subImage.imageRect.extent.width = engine->appState.Renderer.FrameBuffer[0].ColorSwapChain.Width;
		cylinder_layer.subImage.imageRect.extent.height = engine->appState.Renderer.FrameBuffer[0].ColorSwapChain.Height;
		cylinder_layer.subImage.swapchain = engine->appState.Renderer.FrameBuffer[0].ColorSwapChain.Handle;
		cylinder_layer.subImage.imageArrayIndex = 0;
		cylinder_layer.pose.orientation = XrQuaternionf_Multiply(pitch, yaw);
		cylinder_layer.pose.position = pos;
		cylinder_layer.radius = 12.0f;
		cylinder_layer.centralAngle = (float)(M_PI * 0.5);
		cylinder_layer.aspectRatio = VR_GetConfigFloat(VR_CONFIG_CANVAS_ASPECT);

		// Build the cylinder layer
		if (vrMode == VR_MODE_MONO_SCREEN) {
			cylinder_layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
			engine->appState.Layers[engine->appState.LayerCount++].Cylinder = cylinder_layer;
		} else if (engine->appState.Renderer.Multiview) {
			cylinder_layer.eyeVisibility = XR_EYE_VISIBILITY_LEFT;
			engine->appState.Layers[engine->appState.LayerCount++].Cylinder = cylinder_layer;
			cylinder_layer.eyeVisibility = XR_EYE_VISIBILITY_RIGHT;
			cylinder_layer.subImage.imageArrayIndex = 1;
			engine->appState.Layers[engine->appState.LayerCount++].Cylinder = cylinder_layer;
		} else {
			cylinder_layer.eyeVisibility = XR_EYE_VISIBILITY_LEFT;
			engine->appState.Layers[engine->appState.LayerCount++].Cylinder = cylinder_layer;
			cylinder_layer.eyeVisibility = XR_EYE_VISIBILITY_RIGHT;
			cylinder_layer.subImage.swapchain = engine->appState.Renderer.FrameBuffer[1].ColorSwapChain.Handle;
			engine->appState.Layers[engine->appState.LayerCount++].Cylinder = cylinder_layer;
		}
	} else {
		assert(false);
	}

	// Compose the layers for this frame.
	const XrCompositionLayerBaseHeader* layers[ovrMaxLayerCount] = {};
	for (int i = 0; i < engine->appState.LayerCount; i++) {
		layers[i] = (const XrCompositionLayerBaseHeader*)&engine->appState.Layers[i];
	}

	XrFrameEndInfo endFrameInfo = {};
	endFrameInfo.type = XR_TYPE_FRAME_END_INFO;
	endFrameInfo.displayTime = frameState.predictedDisplayTime;
	endFrameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	endFrameInfo.layerCount = engine->appState.LayerCount;
	endFrameInfo.layers = layers;

	OXR(xrEndFrame(engine->appState.Session, &endFrameInfo));
	int instances = engine->appState.Renderer.Multiview ? 1 : ovrMaxNumEyes;
	for (int i = 0; i < instances; i++) {
		ovrFramebuffer* frameBuffer = &engine->appState.Renderer.FrameBuffer[instances];
		frameBuffer->TextureSwapChainIndex++;
		frameBuffer->TextureSwapChainIndex %= frameBuffer->TextureSwapChainLength;
	}
}

int VR_GetConfig( VRConfig config ) {
	return vrConfig[config];
}

void VR_SetConfig( VRConfig config, int value) {
	vrConfig[config] = value;
}

float VR_GetConfigFloat(VRConfigFloat config) {
	return vrConfigFloat[config];
}

void VR_SetConfigFloat(VRConfigFloat config, float value) {
	vrConfigFloat[config] = value;
}

void* VR_BindFramebuffer(engine_t *engine) {
	if (!initialized) return nullptr;
	int fboIndex = VR_GetConfig(VR_CONFIG_CURRENT_FBO);
	return ovrFramebuffer_SetCurrent(&engine->appState.Renderer.FrameBuffer[fboIndex]);
}

XrView VR_GetView(int eye) {
	return projections[eye];
}

XrVector3f VR_GetHMDAngles() {
	return hmdorientation;
}

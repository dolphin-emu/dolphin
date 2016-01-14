// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Flag.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"
#include "VideoCommon/VideoCommon.h"

enum PostProcessingTrigger : u32
{
	POST_PROCESSING_TRIGGER_ON_SWAP,
	POST_PROCESSING_TRIGGER_ON_PROJECTION,
	POST_PROCESSING_TRIGGER_ON_EFB_COPY
};

enum PostProcessingOptionType : u32
{
	POST_PROCESSING_OPTION_TYPE_BOOL,
	POST_PROCESSING_OPTION_TYPE_FLOAT,
	POST_PROCESSING_OPTION_TYPE_INTEGER
};

enum PostProcessingInputType : u32
{
	POST_PROCESSING_INPUT_TYPE_IMAGE,                   // external image loaded from file
	POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER,            // colorbuffer at internal resolution
	POST_PROCESSING_INPUT_TYPE_DEPTH_BUFFER,            // depthbuffer at internal resolution
	POST_PROCESSING_INPUT_TYPE_PREVIOUS_PASS_OUTPUT,    // output of the previous pass
	POST_PROCESSING_INPUT_TYPE_PASS_OUTPUT              // output of a previous pass
};

enum PostProcessingInputFilter : u32
{
	POST_PROCESSING_INPUT_FILTER_NEAREST,               // nearest/point sampling
	POST_PROCESSING_INPUT_FILTER_LINEAR                 // linear sampling
};

enum PostProcessingAddressMode : u32
{
	POST_PROCESSING_ADDRESS_MODE_CLAMP,                 // clamp to edge
	POST_PROCESSING_ADDRESS_MODE_WRAP,                  // wrap around at edge
	POST_PROCESSING_ADDRESS_MODE_BORDER                 // fixed color (0) at edge
};

// Maximum number of texture inputs to a post-processing shader.
static const size_t POST_PROCESSING_MAX_TEXTURE_INPUTS = 4;

// Class containing all options, source, and pass information of a shader
class PostProcessingShaderConfiguration
{
public:
	struct ConfigurationOption final
	{
		bool m_bool_value;
		bool m_default_bool_value;

		std::vector<float> m_default_float_values;
		std::vector<s32> m_default_integer_values;

		std::vector<float> m_float_values;
		std::vector<s32> m_integer_values;

		std::vector<float> m_float_min_values;
		std::vector<s32> m_integer_min_values;

		std::vector<float> m_float_max_values;
		std::vector<s32> m_integer_max_values;

		std::vector<float> m_float_step_values;
		std::vector<s32> m_integer_step_values;

		PostProcessingOptionType m_type;

		std::string m_gui_name;
		std::string m_option_name;
		std::string m_dependent_option;

		bool m_compile_time_constant;

		bool m_dirty;
	};

	struct RenderPass final
	{
		struct Input final
		{
			PostProcessingInputType type;
			PostProcessingInputFilter filter;
			PostProcessingAddressMode address_mode;
			u32 texture_unit;
			u32 pass_output_index;

			std::unique_ptr<u8[]> external_image_data;
			TargetSize external_image_size;
		};

		std::vector<Input> inputs;
		std::string entry_point;
		std::string dependent_option;
		float output_scale;
	};

	using ConfigMap = std::map<std::string, ConfigurationOption>;
	using RenderPassList = std::vector<RenderPass>;

	PostProcessingShaderConfiguration() = default;
	virtual ~PostProcessingShaderConfiguration() {}

	// Loads the configuration with a shader
	bool LoadShader(const std::string& sub_dir, const std::string& name);
	void SaveOptionsConfiguration();
	const std::string& GetShaderName() const { return m_shader_name; }
	const std::string& GetShaderSource() const { return m_shader_source; }

	bool IsDirty() const { return m_any_options_dirty; }
	bool IsCompileTimeConstantsDirty() const { return m_compile_time_constants_dirty; }
	void SetDirty() { m_any_options_dirty = true; }
	void ClearDirty();

	bool RequiresDepthBuffer() const { return m_requires_depth_buffer; }

	bool HasOptions() const { return !m_options.empty(); }
	ConfigMap& GetOptions() { return m_options; }
	const ConfigMap& GetOptions() const { return m_options; }
	const ConfigurationOption& GetOption(const std::string& option) { return m_options[option]; }

	const RenderPassList& GetPasses() const { return m_render_passes; }
	const RenderPass& GetPass(size_t index) const { return m_render_passes.at(index); }

	// For updating option's values
	void SetOptionf(const std::string& option, int index, float value);
	void SetOptioni(const std::string& option, int index, s32 value);
	void SetOptionb(const std::string& option, bool value);

	// Get a list of available post-processing shaders.
	static std::vector<std::string> GetAvailableShaderNames(const std::string& sub_dir);

private:
	struct ConfigBlock final
	{
		std::string m_type;
		std::vector<std::pair<std::string, std::string>> m_options;
	};

	using StringOptionList = std::vector<ConfigBlock>;

	bool m_any_options_dirty = false;
	bool m_compile_time_constants_dirty = false;
	bool m_requires_depth_buffer = false;
	std::string m_shader_name;
	std::string m_shader_source;
	ConfigMap m_options;
	RenderPassList m_render_passes;

	bool ParseShader(const std::string& dirname, const std::string& path);
	bool ParseConfiguration(const std::string& dirname, const std::string& configuration_string);

	std::vector<ConfigBlock> ReadConfigSections(const std::string& configuration_string);
	bool ParseConfigSections(const std::string& dirname, const std::vector<ConfigBlock>& config_blocks);

	bool ParseOptionBlock(const std::string& dirname, const ConfigBlock& block);
	bool ParsePassBlock(const std::string& dirname, const ConfigBlock& block);
	bool LoadExternalImage(const std::string& path, RenderPass::Input* input);

	void CreateDefaultPass();

	void LoadOptionsConfiguration();

	static bool ValidatePassInputs(const RenderPass& pass);
};

class PostProcessor
{
public:
	// Size of the constant buffer reserved for post-processing.
	// 4KiB = 256 constants, a shader should not have this many options.
	static const size_t UNIFORM_BUFFER_SIZE = 4096;

	// List of texture sizes for shader inputs, used to update uniforms.
	using InputTextureSizeArray = std::array<TargetSize, POST_PROCESSING_MAX_TEXTURE_INPUTS>;

	// Constructor needed for timer object
	PostProcessor();
	virtual ~PostProcessor();

	// Get the config for a shader in the current chain.
	PostProcessingShaderConfiguration* GetPostShaderConfig(const std::string& shader_name);

	// Get the current scaling shader config.
	PostProcessingShaderConfiguration* GetScalingShaderConfig() { return m_scaling_config.get(); }

	bool IsActive() const { return m_active; }
	bool RequiresDepthBuffer() const { return m_requires_depth_buffer; }

	void SetReloadFlag() { m_reload_flag.Set(); }
	bool RequiresReload() { return m_reload_flag.TestAndClear(); }

	void OnProjectionLoaded(u32 type);
	void OnEFBCopy();
	void OnEndFrame();

	// Should be implemented by the backends for backend specific code
	virtual bool Initialize() = 0;
	virtual void ReloadShaders() = 0;

	// Used when post-processing on perspective->ortho switch.
	virtual void PostProcessEFB() = 0;

	// Copy/resize src_texture to dst_texture (0 means backbuffer), using the resize/blit shader.
	virtual void BlitToFramebuffer(const TargetRectangle& dst_rect, const TargetSize& dst_size, uintptr_t dst_texture,
								   const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture,
								   int src_layer) = 0;

	// Post-process an image.
	virtual void PostProcess(const TargetRectangle& visible_rect, const TargetSize& tex_size, int tex_layers,
							 uintptr_t texture, uintptr_t depth_texture) = 0;

	// Construct the options uniform buffer source for the specified config.
	static std::string GetUniformBufferShaderSource(API_TYPE api, const PostProcessingShaderConfiguration* config);

	// Construct a complete fragment shader (HLSL/GLSL) for the specified pass.
	static std::string GetPassFragmentShaderSource(API_TYPE api, const PostProcessingShaderConfiguration* config,
												   const PostProcessingShaderConfiguration::RenderPass* pass);

	// Scale a target resolution to an output's scale
	static TargetSize ScaleTargetSize(const TargetSize& orig_size, float scale);

	// Scale a target rectangle to an output's scale
	static TargetRectangle ScaleTargetRectangle(API_TYPE api, const TargetRectangle& src, float scale);


protected:
	enum PROJECTION_STATE : u32
	{
		PROJECTION_STATE_INITIAL,
		PROJECTION_STATE_PERSPECTIVE,
		PROJECTION_STATE_FINAL
	};

	// Update constant buffer with the current values from the config.
	void UpdateUniformBuffer(API_TYPE api, const PostProcessingShaderConfiguration* config,
							 void* buffer_ptr, const InputTextureSizeArray& input_sizes,
							 const TargetSize& target_size, const TargetRectangle& src_rect,
							 const TargetSize& src_size, int src_layer);

	// Load m_configs with the selected post-processing shaders.
	void ReloadShaderConfigs();
	void ReloadPostProcessingShaderConfigs();
	void ReloadScalingShaderConfig();
	void ReloadStereoShaderConfig();

	// Timer for determining our time value
	Common::Timer m_timer;

	// Set by UI thread when the shader changes
	Common::Flag m_reload_flag;

	// List of current post-processing shaders, ordered by application order
	std::vector<std::string> m_shader_names;

	// Current post-processing shader config
	std::unordered_map<std::string, std::unique_ptr<PostProcessingShaderConfiguration>> m_shader_configs;

	// Scaling shader config
	std::unique_ptr<PostProcessingShaderConfiguration> m_scaling_config;

	// Stereo shader config
	std::unique_ptr<PostProcessingShaderConfiguration> m_stereo_config;

	// Projection state for detecting when to apply post
	PROJECTION_STATE m_projection_state = PROJECTION_STATE_INITIAL;

	// Global post-processing enable state
	bool m_active = false;
	bool m_requires_depth_buffer = false;

	// common shader code between backends
	static const std::string s_post_fragment_header_ogl;
	static const std::string s_post_fragment_header_d3d;
	static const std::string s_post_fragment_header_common;
};

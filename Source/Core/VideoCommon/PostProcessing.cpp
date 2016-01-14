// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include <sstream>
#include <string>

#include <SOIL/SOIL.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

static const char s_default_shader[] = "void main() { SetOutput(Sample()); }\n";

std::vector<std::string> PostProcessingShaderConfiguration::GetAvailableShaderNames(const std::string& sub_dir)
{
	const std::vector<std::string> search_dirs = { File::GetUserPath(D_SHADERS_IDX) + sub_dir, File::GetSysDirectory() + SHADERS_DIR DIR_SEP + sub_dir };
	const std::vector<std::string> search_extensions = { ".glsl" };
	std::vector<std::string> result;
	std::vector<std::string> paths;

	// main folder
	paths = DoFileSearch(search_extensions, search_dirs, false);
	for (const std::string& path : paths)
	{
		std::string filename;
		if (SplitPath(path, nullptr, &filename, nullptr))
		{
			if (std::find(result.begin(), result.end(), filename) == result.end())
				result.push_back(filename);
		}
	}

	// folders/sub-shaders
	paths = FindSubdirectories(search_dirs, false);
	for (const std::string& dirname : paths)
	{
		// find sub-shaders in this folder
		size_t pos = dirname.find_last_of(DIR_SEP_CHR);
		if (pos != std::string::npos && (pos != dirname.length() - 1))
		{
			std::string shader_dirname = dirname.substr(pos + 1);
			std::vector<std::string> sub_paths = DoFileSearch(search_extensions, { dirname }, false);
			for (const std::string& sub_path : sub_paths)
			{
				std::string filename;
				if (SplitPath(sub_path, nullptr, &filename, nullptr))
				{
					// Remove /main for main shader
					std::string name = (!strcasecmp(filename.c_str(), "main")) ? (shader_dirname) : (shader_dirname + DIR_SEP + filename);
					if (std::find(result.begin(), result.end(), filename) == result.end())
						result.push_back(name);
				}
			}
		}
	}

	// sort lexicographically
	std::sort(result.begin(), result.end());
	return result;
}

bool PostProcessingShaderConfiguration::LoadShader(const std::string& sub_dir, const std::string& name)
{
	// clear all state
	m_shader_name = name;
	m_shader_source.clear();
	m_options.clear();
	m_render_passes.clear();
	m_any_options_dirty = false;
	m_compile_time_constants_dirty = false;
	m_requires_depth_buffer = false;

	// special case: default shader, no path, use inbuilt code
	if (name.empty())
	{
		m_shader_source = s_default_shader;
		return ParseConfiguration(sub_dir, "");
	}

	// This is kind of horrifying, but we only want to load one shader, in a specific order.
	// (if there's an error, this class is left in an unknown state)
	std::string dirname;
	std::string filename;
	bool found = false;
	bool result = false;

	// Try old-style shaders first, but completely skip if the name is a sub-shader
	if (name.find(DIR_SEP_CHR) == std::string::npos)
	{
		// User/Shaders/sub_dir/<name>.glsl
		dirname = File::GetUserPath(D_SHADERS_IDX) + sub_dir + DIR_SEP;
		filename = dirname + name + ".glsl";
		if (File::Exists(filename))
		{
			result = ParseShader(dirname, filename);
			found = true;
		}
		else
		{
			// Sys/Shaders/sub_dir/<name>.glsl
			dirname = File::GetSysDirectory() + SHADERS_DIR DIR_SEP + sub_dir + DIR_SEP;
			filename = dirname + name + ".glsl";
			if (File::Exists(filename))
			{
				result = ParseShader(dirname, filename);
				found = true;
			}
		}
	}

	// Try shader directories/sub-shaders
	if (!found)
	{
		std::string shader_name;
		std::string sub_shader_name;
		size_t sep_pos = name.find(DIR_SEP_CHR);
		if (sep_pos != std::string::npos && (sep_pos != name.length() - 1))
		{
			shader_name = name.substr(0, sep_pos);
			sub_shader_name = name.substr(sep_pos + 1);
		}
		else
		{
			// default name is main.glsl
			shader_name = name;
			sub_shader_name = "main";
		}

		// User/Shaders/sub_dir/<shader_name>/<sub_shader_name>.glsl
		dirname = File::GetUserPath(D_SHADERS_IDX) + sub_dir + DIR_SEP + shader_name + DIR_SEP;
		filename = dirname + sub_shader_name + ".glsl";
		if (File::Exists(filename))
		{
			result = ParseShader(dirname, filename);
			found = true;
		}
		else
		{
			// Sys/Shaders/sub_dir/<shader_name>/<sub_shader_name>.glsl
			dirname = File::GetSysDirectory() + SHADERS_DIR DIR_SEP + sub_dir + DIR_SEP + shader_name + DIR_SEP;
			filename = dirname + sub_shader_name + ".glsl";
			if (File::Exists(filename))
			{
				result = ParseShader(dirname, filename);
				found = true;
			}
		}
	}

	if (!found)
	{
		ERROR_LOG(VIDEO, "Post-processing shader not found: %s", name.c_str());
		return false;
	}

	if (!result)
	{
		ERROR_LOG(VIDEO, "Failed to parse post-processing shader at %s", filename.c_str());
		return false;
	}

	LoadOptionsConfiguration();
	return true;
}

bool PostProcessingShaderConfiguration::ParseShader(const std::string& dirname, const std::string& path)
{
	// Read to a single string we can work with
	std::string code;
	if (!File::ReadFileToString(path, code))
		return false;

	// Find configuration block, if any
	const std::string config_start_delimiter = "[configuration]";
	const std::string config_end_delimiter = "[/configuration]";
	size_t configuration_start = code.find(config_start_delimiter);
	size_t configuration_end = code.find(config_end_delimiter);
	std::string configuration_string;

	if (configuration_start != std::string::npos && configuration_end != std::string::npos)
	{
		// Remove the configuration area from the source string, leaving only the GLSL code.
		m_shader_source = code;
		m_shader_source.erase(configuration_start, (configuration_end - configuration_start + config_end_delimiter.length()));

		// Extract configuration string, and parse options/passes
		configuration_string = code.substr(configuration_start + config_start_delimiter.size(),
										   configuration_end - configuration_start - config_start_delimiter.size());
	}
	else
	{
		// If there is no configuration block. Assume the entire file is code.
		m_shader_source = code;
	}

	return ParseConfiguration(dirname, configuration_string);
}

bool PostProcessingShaderConfiguration::ParseConfiguration(const std::string& dirname, const std::string& configuration_string)
{
	std::vector<ConfigBlock> config_blocks = ReadConfigSections(configuration_string);
	if (!ParseConfigSections(dirname, config_blocks))
		return false;

	// If no render passes are specified, create a default pass.
	if (m_render_passes.empty())
		CreateDefaultPass();

	return true;
}

std::vector<PostProcessingShaderConfiguration::ConfigBlock> PostProcessingShaderConfiguration::ReadConfigSections(const std::string& configuration_string)
{
	std::istringstream in(configuration_string);

	std::vector<ConfigBlock> config_blocks;
	ConfigBlock* current_block = nullptr;

	while (!in.eof())
	{
		std::string line;

		if (std::getline(in, line))
		{
#ifndef _WIN32
			// Check for CRLF eol and convert it to LF
			if (!line.empty() && line.at(line.size() - 1) == '\r')
				line.erase(line.size() - 1);
#endif

			if (line.size() > 0)
			{
				if (line[0] == '[')
				{
					size_t endpos = line.find("]");

					if (endpos != std::string::npos)
					{
						std::string sub = line.substr(1, endpos - 1);
						ConfigBlock section;
						section.m_type = sub;
						config_blocks.push_back(std::move(section));
						current_block = &config_blocks.back();
					}
				}
				else
				{
					std::string key, value;
					IniFile::ParseLine(line, &key, &value);
					if (!key.empty() && !value.empty())
					{
						if (current_block)
							current_block->m_options.emplace_back(key, value);
					}
				}
			}
		}
	}

	return config_blocks;
}

bool PostProcessingShaderConfiguration::ParseConfigSections(const std::string& dirname, const std::vector<ConfigBlock>& config_blocks)
{
	for (const ConfigBlock& option : config_blocks)
	{
		if (option.m_type == "Pass")
		{
			if (!ParsePassBlock(dirname, option))
				return false;
		}
		else
		{
			if (!ParseOptionBlock(dirname, option))
				return false;
		}
	}

	return true;
}

bool PostProcessingShaderConfiguration::ParseOptionBlock(const std::string& dirname, const ConfigBlock& block)
{
	// Initialize to default values, in case the configuration section is incomplete.
	ConfigurationOption option;
	option.m_bool_value = false;
	option.m_type = POST_PROCESSING_OPTION_TYPE_FLOAT;
	option.m_compile_time_constant = false;
	option.m_dirty = false;

	if (block.m_type == "OptionBool")
	{
		option.m_type = POST_PROCESSING_OPTION_TYPE_BOOL;
	}
	else if (block.m_type == "OptionRangeFloat")
	{
		option.m_type = POST_PROCESSING_OPTION_TYPE_FLOAT;
	}
	else if (block.m_type == "OptionRangeInteger")
	{
		option.m_type = POST_PROCESSING_OPTION_TYPE_INTEGER;
	}
	else
	{
		// not fatal, provided for forwards compatibility
		WARN_LOG(VIDEO, "Unknown section name in post-processing shader config: '%s'", block.m_type.c_str());
		return true;
	}

	for (const auto& string_option : block.m_options)
	{
		if (string_option.first == "GUIName")
		{
			option.m_gui_name = string_option.second;
		}
		else if (string_option.first == "OptionName")
		{
			option.m_option_name = string_option.second;
		}
		else if (string_option.first == "DependentOption")
		{
			option.m_dependent_option = string_option.second;
		}
		else if (string_option.first == "ResolveAtCompilation")
		{
			TryParse(string_option.second, &option.m_compile_time_constant);
		}
		else if (string_option.first == "MinValue" ||
					string_option.first == "MaxValue" ||
					string_option.first == "DefaultValue" ||
					string_option.first == "StepAmount")
		{
			std::vector<s32>* output_integer = nullptr;
			std::vector<float>* output_float = nullptr;

			if (string_option.first == "MinValue")
			{
				output_integer = &option.m_integer_min_values;
				output_float = &option.m_float_min_values;
			}
			else if (string_option.first == "MaxValue")
			{
				output_integer = &option.m_integer_max_values;
				output_float = &option.m_float_max_values;
			}
			else if (string_option.first == "DefaultValue")
			{
				output_integer = &option.m_integer_values;
				output_float = &option.m_float_values;
			}
			else if (string_option.first == "StepAmount")
			{
				output_integer = &option.m_integer_step_values;
				output_float = &option.m_float_step_values;
			}

			if (option.m_type == POST_PROCESSING_OPTION_TYPE_BOOL)
			{
				TryParse(string_option.second, &option.m_bool_value);
			}
			else if (option.m_type == POST_PROCESSING_OPTION_TYPE_INTEGER)
			{
				TryParseVector(string_option.second, output_integer);
				if (output_integer->size() > 4)
					output_integer->erase(output_integer->begin() + 4, output_integer->end());
			}
			else if (option.m_type == POST_PROCESSING_OPTION_TYPE_FLOAT)
			{
				TryParseVector(string_option.second, output_float);
				if (output_float->size() > 4)
					output_float->erase(output_float->begin() + 4, output_float->end());
			}
		}
	}
	option.m_default_bool_value = option.m_bool_value;
	option.m_default_float_values = option.m_float_values;
	option.m_default_integer_values = option.m_integer_values;
	m_options[option.m_option_name] = option;
	return true;
}

bool PostProcessingShaderConfiguration::ParsePassBlock(const std::string& dirname, const ConfigBlock& block)
{
	RenderPass pass;
	pass.output_scale = 1.0f;

	for (const auto& option : block.m_options)
	{
		const std::string& key = option.first;
		const std::string& value = option.second;
		if (key == "EntryPoint")
		{
			pass.entry_point = value;
		}
		else if (key == "OutputScale")
		{
			TryParse(value, &pass.output_scale);
			if (pass.output_scale <= 0.0f)
				return false;
		}
		else if (key == "OutputScaleNative")
		{
			TryParse(value, &pass.output_scale);
			if (pass.output_scale <= 0.0f)
				return false;

			// negative means native scale
			pass.output_scale = -pass.output_scale;
		}
		else if (key == "DependantOption")
		{
			const auto& dependant_option = m_options.find(value);
			if (dependant_option == m_options.end())
			{
				ERROR_LOG(VIDEO, "Post processing configuration error: Unknown dependant option: %s", value.c_str());
				return false;
			}

			pass.dependent_option = value;
		}
		else if (key.compare(0, 5, "Input") == 0 && key.length() > 5)
		{
			u32 texture_unit = key[5] - '0';
			if (texture_unit > POST_PROCESSING_MAX_TEXTURE_INPUTS)
			{
				ERROR_LOG(VIDEO, "Post processing configuration error: Out-of-range texture unit: %u", texture_unit);
				return false;
			}

			// Input declared yet?
			RenderPass::Input* input = nullptr;
			for (RenderPass::Input& input_it : pass.inputs)
			{
				if (input_it.texture_unit == texture_unit)
				{
					input = &input_it;
					break;
				}
			}
			if (input == nullptr)
			{
				RenderPass::Input new_input;
				new_input.type = POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER;
				new_input.filter = POST_PROCESSING_INPUT_FILTER_LINEAR;
				new_input.address_mode = POST_PROCESSING_ADDRESS_MODE_BORDER;
				new_input.texture_unit = texture_unit;
				new_input.pass_output_index = 0;
				pass.inputs.push_back(std::move(new_input));
				input = &pass.inputs.back();
			}

			// Input#(Filter|Mode|Source)
			std::string extra = (key.length() > 6) ? key.substr(6) : "";
			if (extra.empty())
			{
				// Type
				if (value == "ColorBuffer")
				{
					input->type = POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER;
				}
				else if (value == "DepthBuffer")
				{
					input->type = POST_PROCESSING_INPUT_TYPE_DEPTH_BUFFER;
					m_requires_depth_buffer = true;
				}
				else if (value == "PreviousPass")
				{
					input->type = POST_PROCESSING_INPUT_TYPE_PREVIOUS_PASS_OUTPUT;
				}
				else if (value == "Image")
				{
					input->type = POST_PROCESSING_INPUT_TYPE_IMAGE;
				}
				else if (value.compare(0, 4, "Pass") == 0)
				{
					input->type = POST_PROCESSING_INPUT_TYPE_PASS_OUTPUT;
					if (!TryParse(value.substr(4), &input->pass_output_index) || input->pass_output_index >= m_render_passes.size())
					{
						ERROR_LOG(VIDEO, "Post processing configuration error: Out-of-range render pass reference: %u", input->pass_output_index);
						return false;
					}
				}
				else
				{
					ERROR_LOG(VIDEO, "Post processing configuration error: Invalid input type: %s", value.c_str());
					return false;
				}
			}
			else if (extra == "Filter")
			{
				if (value == "Nearest")
				{
					input->filter = POST_PROCESSING_INPUT_FILTER_NEAREST;
				}
				else if (value == "Linear")
				{
					input->filter = POST_PROCESSING_INPUT_FILTER_LINEAR;
				}
				else
				{
					ERROR_LOG(VIDEO, "Post processing configuration error: Invalid input filter: %s", value.c_str());
					return false;
				}
			}
			else if (extra == "Mode")
			{
				if (value == "Clamp")
				{
					input->address_mode = POST_PROCESSING_ADDRESS_MODE_CLAMP;
				}
				else if (value == "Wrap")
				{
					input->address_mode = POST_PROCESSING_ADDRESS_MODE_WRAP;
				}
				else if (value == "Border")
				{
					input->address_mode = POST_PROCESSING_ADDRESS_MODE_BORDER;
				}
				else
				{
					ERROR_LOG(VIDEO, "Post processing configuration error: Invalid input mode: %s", value.c_str());
					return false;
				}
			}
			else if (extra == "Source")
			{
				// Load external image
				std::string path = dirname + value;
				if (!File::Exists(path) || !LoadExternalImage(path, input))
				{
					ERROR_LOG(VIDEO, "Post processing configuration error: Unable to load external image at '%s'", value.c_str());
					return false;
				}
			}
			else
			{
				ERROR_LOG(VIDEO, "Post processing configuration error: Unknown input key: %s", key.c_str());
				return false;
			}
		}
	}

	if (!ValidatePassInputs(pass))
		return false;

	m_render_passes.push_back(std::move(pass));
	return true;
}

bool PostProcessingShaderConfiguration::LoadExternalImage(const std::string& path, RenderPass::Input* input)
{
	File::IOFile file(path, "rb");
	std::vector<u8> buffer(file.GetSize());
	if (!file.IsOpen() || !file.ReadBytes(buffer.data(), file.GetSize()))
		return false;

	int image_width;
	int image_height;
	int image_channels;
	u8* decoded = SOIL_load_image_from_memory(buffer.data(), (int)buffer.size(), &image_width, &image_height, &image_channels, SOIL_LOAD_RGBA);
	if (decoded == nullptr)
		return false;

	// Reallocate the memory so we can manage it
	input->type = POST_PROCESSING_INPUT_TYPE_IMAGE;
	input->external_image_size.width = image_width;
	input->external_image_size.height = image_height;
	input->external_image_data = std::make_unique<u8[]>(image_width * image_height * 4);
	memcpy(input->external_image_data.get(), decoded, image_width * image_height * 4);
	SOIL_free_image_data(decoded);
	return true;
}

bool PostProcessingShaderConfiguration::ValidatePassInputs(const RenderPass& pass)
{
	return std::all_of(pass.inputs.begin(), pass.inputs.end(), [&pass](const RenderPass::Input& input)
	{
		// Check for image inputs without valid data
		if (input.type == POST_PROCESSING_INPUT_TYPE_IMAGE && !input.external_image_data)
		{
			ERROR_LOG(VIDEO, "Post processing configuration error: Pass '%s' input %u is missing image source.", pass.entry_point.c_str(), input.texture_unit);
			return false;
		}

		return true;
	});
}

void PostProcessingShaderConfiguration::CreateDefaultPass()
{
	RenderPass::Input input;
	input.type = POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER;
	input.filter = POST_PROCESSING_INPUT_FILTER_LINEAR;
	input.address_mode = POST_PROCESSING_ADDRESS_MODE_CLAMP;
	input.texture_unit = 0;
	input.pass_output_index = 0;

	RenderPass pass;
	pass.entry_point = "main";
	pass.inputs.push_back(std::move(input));
	pass.output_scale = 1;
	m_render_passes.push_back(std::move(pass));
}

void PostProcessingShaderConfiguration::LoadOptionsConfiguration()
{
	IniFile ini;
	ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	std::string section = m_shader_name + "-options";

	for (auto& it : m_options)
	{
		switch (it.second.m_type)
		{
		case POST_PROCESSING_OPTION_TYPE_BOOL:
			ini.GetOrCreateSection(section)->Get(it.second.m_option_name, &it.second.m_bool_value, it.second.m_bool_value);
			break;

		case POST_PROCESSING_OPTION_TYPE_INTEGER:
			{
				std::string value;
				ini.GetOrCreateSection(section)->Get(it.second.m_option_name, &value);
				if (value != "")
					TryParseVector(value, &it.second.m_integer_values);
			}
			break;

		case POST_PROCESSING_OPTION_TYPE_FLOAT:
			{
				std::string value;
				ini.GetOrCreateSection(section)->Get(it.second.m_option_name, &value);
				if (value != "")
					TryParseVector(value, &it.second.m_float_values);
			}
			break;
		}
	}
}

void PostProcessingShaderConfiguration::SaveOptionsConfiguration()
{
	IniFile ini;
	ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	std::string section = m_shader_name + "-options";

	for (auto& it : m_options)
	{
		switch (it.second.m_type)
		{
		case POST_PROCESSING_OPTION_TYPE_BOOL:
			ini.GetOrCreateSection(section)->Set(it.second.m_option_name, it.second.m_bool_value);
			break;

		case POST_PROCESSING_OPTION_TYPE_INTEGER:
			{
				std::string value = "";
				for (size_t i = 0; i < it.second.m_integer_values.size(); ++i)
					value += StringFromFormat("%d%s", it.second.m_integer_values[i], i == (it.second.m_integer_values.size() - 1) ? "": ", ");
				ini.GetOrCreateSection(section)->Set(it.second.m_option_name, value);
			}
			break;

		case POST_PROCESSING_OPTION_TYPE_FLOAT:
			{
				std::ostringstream value;
				value.imbue(std::locale("C"));

				for (size_t i = 0; i < it.second.m_float_values.size(); ++i)
				{
					value << it.second.m_float_values[i];
					if (i != (it.second.m_float_values.size() - 1))
						value << ", ";
				}
				ini.GetOrCreateSection(section)->Set(it.second.m_option_name, value.str());
			}
			break;
		}
	}

	ini.Save(File::GetUserPath(F_DOLPHINCONFIG_IDX));
}

void PostProcessingShaderConfiguration::ClearDirty()
{
	m_any_options_dirty = false;
	m_compile_time_constants_dirty = false;
	for (auto& it : m_options)
		it.second.m_dirty = false;
}

void PostProcessingShaderConfiguration::SetOptionf(const std::string& option, int index, float value)
{
	auto it = m_options.find(option);
	if (it->second.m_float_values[index] == value)
		return;

	it->second.m_float_values[index] = value;
	it->second.m_dirty = true;
	m_any_options_dirty = true;

	if (it->second.m_compile_time_constant)
		m_compile_time_constants_dirty = true;
}

void PostProcessingShaderConfiguration::SetOptioni(const std::string& option, int index, s32 value)
{
	auto it = m_options.find(option);
	if (it->second.m_integer_values[index] == value)
		return;

	it->second.m_integer_values[index] = value;
	it->second.m_dirty = true;
	m_any_options_dirty = true;

	if (it->second.m_compile_time_constant)
		m_compile_time_constants_dirty = true;
}

void PostProcessingShaderConfiguration::SetOptionb(const std::string& option, bool value)
{
	auto it = m_options.find(option);
	if (it->second.m_bool_value == value)
		return;

	it->second.m_bool_value = value;
	it->second.m_dirty = true;
	m_any_options_dirty = true;

	if (it->second.m_compile_time_constant)
		m_compile_time_constants_dirty = true;
}

PostProcessor::PostProcessor()
{
	m_timer.Start();
}

PostProcessor::~PostProcessor()
{
	m_timer.Stop();
}

void PostProcessor::OnProjectionLoaded(u32 type)
{
	if (!m_active || !g_ActiveConfig.bPostProcessingEnable ||
		(g_ActiveConfig.iPostProcessingTrigger != POST_PROCESSING_TRIGGER_ON_PROJECTION &&
		(g_ActiveConfig.iPostProcessingTrigger != POST_PROCESSING_TRIGGER_ON_EFB_COPY)))
	{
		return;
	}

	if (type == GX_PERSPECTIVE)
	{
		// Only adjust the flag if this is our first perspective load.
		if (m_projection_state == PROJECTION_STATE_INITIAL)
			m_projection_state = PROJECTION_STATE_PERSPECTIVE;
	}
	else if (type == GX_ORTHOGRAPHIC)
	{
		// Fire off postprocessing on the current efb if a perspective scene has been drawn.
		if (g_ActiveConfig.iPostProcessingTrigger == POST_PROCESSING_TRIGGER_ON_PROJECTION &&
			m_projection_state == PROJECTION_STATE_PERSPECTIVE)
		{
			m_projection_state = PROJECTION_STATE_FINAL;
			PostProcessEFB();
		}
	}
}

void PostProcessor::OnEFBCopy()
{
	if (!m_active || !g_ActiveConfig.bPostProcessingEnable ||
		g_ActiveConfig.iPostProcessingTrigger != POST_PROCESSING_TRIGGER_ON_EFB_COPY)
	{
		return;
	}

	// Fire off postprocessing on the current efb if a perspective scene has been drawn.
	if (m_projection_state == PROJECTION_STATE_PERSPECTIVE)
	{
		m_projection_state = PROJECTION_STATE_FINAL;
		PostProcessEFB();
	}
}

void PostProcessor::OnEndFrame()
{
	if (!m_active || !g_ActiveConfig.bPostProcessingEnable ||
		(g_ActiveConfig.iPostProcessingTrigger != POST_PROCESSING_TRIGGER_ON_PROJECTION &&
		(g_ActiveConfig.iPostProcessingTrigger != POST_PROCESSING_TRIGGER_ON_EFB_COPY)))
	{
		return;
	}

	// If we didn't switch to orthographic after perspective, post-process now (e.g. if no HUD was drawn)
	if (m_projection_state == PROJECTION_STATE_PERSPECTIVE)
		PostProcessEFB();

	m_projection_state = PROJECTION_STATE_INITIAL;
}

void PostProcessor::UpdateUniformBuffer(API_TYPE api, const PostProcessingShaderConfiguration* config,
										void* buffer_ptr, const InputTextureSizeArray& input_sizes,
										const TargetSize& target_size, const TargetRectangle& src_rect,
										const TargetSize& src_size, int src_layer)
{
	// Each option is aligned to a float4
	union Constant
	{
		int bool_constant;
		float float_constant[4];
		s32 int_constant[4];
	};

	Constant* constants = reinterpret_cast<Constant*>(buffer_ptr);

	// float4 input_resolutions[4]
	for (size_t i = 0; i < POST_PROCESSING_MAX_TEXTURE_INPUTS; i++)
	{
		constants[i].float_constant[0] = (float)input_sizes[i].width;
		constants[i].float_constant[1] = (float)input_sizes[i].height;
		constants[i].float_constant[2] = 1.0f / (float)input_sizes[i].width;
		constants[i].float_constant[3] = 1.0f / (float)input_sizes[i].height;
	}

	// float4 src_rect (only used by GL)
	u32 constant_idx = POST_PROCESSING_MAX_TEXTURE_INPUTS;
	constants[constant_idx].float_constant[0] = (float)src_rect.left / (float)src_size.width;
	constants[constant_idx].float_constant[1] = (float)((api == API_OPENGL) ? src_rect.bottom : src_rect.top) / (float)src_size.height;
	constants[constant_idx].float_constant[2] = (float)src_rect.GetWidth() / (float)src_size.width;
	constants[constant_idx].float_constant[3] = (float)src_rect.GetHeight() / (float)src_size.height;
	constant_idx++;

	// float4 target_resolution
	constants[constant_idx].float_constant[0] = (float)target_size.width;
	constants[constant_idx].float_constant[1] = (float)target_size.height;
	constants[constant_idx].float_constant[2] = 1.0f / (float)target_size.width;
	constants[constant_idx].float_constant[3] = 1.0f / (float)target_size.height;
	constant_idx++;

	// float time, float layer
	constants[constant_idx].float_constant[0] = float(double(m_timer.GetTimeDifference()) / 1000.0);
	constants[constant_idx].float_constant[1] = float(std::max(src_layer, 0));
	constants[constant_idx].float_constant[2] = 0.0f;
	constants[constant_idx].float_constant[3] = 0.0f;
	constant_idx++;

	// Set from options. This is an ordered map so it will always match the order in the shader code generated.
	for (const auto& it : config->GetOptions())
	{
		// Skip compile-time constants, since they're set in the program source.
		if (it.second.m_compile_time_constant)
			continue;

		switch (it.second.m_type)
		{
		case POST_PROCESSING_OPTION_TYPE_BOOL:
			constants[constant_idx].int_constant[0] = (int)it.second.m_bool_value;
			constants[constant_idx].int_constant[1] = 0;
			constants[constant_idx].int_constant[2] = 0;
			constants[constant_idx].int_constant[3] = 0;
			constant_idx++;
			break;

		case POST_PROCESSING_OPTION_TYPE_INTEGER:
			{
				u32 components = std::max((u32)it.second.m_integer_values.size(), (u32)0);
				for (u32 i = 0; i < components; i++)
					constants[constant_idx].int_constant[i] = it.second.m_integer_values[i];
				for (u32 i = components; i < 4; i++)
					constants[constant_idx].int_constant[i] = 0;

				constant_idx++;
			}
			break;

		case POST_PROCESSING_OPTION_TYPE_FLOAT:
			{
				u32 components = std::max((u32)it.second.m_float_values.size(), (u32)0);
				for (u32 i = 0; i < components; i++)
					constants[constant_idx].float_constant[i] = it.second.m_float_values[i];
				for (u32 i = components; i < 4; i++)
					constants[constant_idx].int_constant[i] = 0;

				constant_idx++;
			}
			break;
		}
	}

	// Sanity check, should never fail
	_dbg_assert_(VIDEO, constant_idx <= (UNIFORM_BUFFER_SIZE / 16));
}

PostProcessingShaderConfiguration* PostProcessor::GetPostShaderConfig(const std::string& shader_name)
{
	const auto& it = m_shader_configs.find(shader_name);
	if (it == m_shader_configs.end())
		return nullptr;

	return it->second.get();
}

void PostProcessor::ReloadShaderConfigs()
{
	ReloadPostProcessingShaderConfigs();
	ReloadScalingShaderConfig();
	ReloadStereoShaderConfig();
}

void PostProcessor::ReloadPostProcessingShaderConfigs()
{
	// Load post-processing shader list
	m_shader_names.clear();
	SplitString(g_ActiveConfig.sPostProcessingShaders, ':', m_shader_names);

	// Load shaders
	m_shader_configs.clear();
	m_requires_depth_buffer = false;
	for (const std::string& shader_name : m_shader_names)
	{
		// Shaders can be repeated. In this case, only load it once.
		if (m_shader_configs.find(shader_name) != m_shader_configs.end())
			continue;

		// Load this shader.
		std::unique_ptr<PostProcessingShaderConfiguration> shader_config = std::make_unique<PostProcessingShaderConfiguration>();
		if (!shader_config->LoadShader(POSTPROCESSING_SHADER_SUBDIR, shader_name))
		{
			ERROR_LOG(VIDEO, "Failed to load postprocessing shader ('%s'). This shader will be ignored.", shader_name.c_str());
			OSD::AddMessage(StringFromFormat("Failed to load postprocessing shader ('%s'). This shader will be ignored.", shader_name.c_str()));
			continue;
		}

		// Store in map for use by backend
		m_requires_depth_buffer |= shader_config->RequiresDepthBuffer();
		m_shader_configs[shader_name] = std::move(shader_config);
	}
}

void PostProcessor::ReloadScalingShaderConfig()
{
	m_scaling_config = std::make_unique<PostProcessingShaderConfiguration>();
	if (!m_scaling_config->LoadShader(SCALING_SHADER_SUBDIR, g_ActiveConfig.sScalingShader))
	{
		ERROR_LOG(VIDEO, "Failed to load scaling shader ('%s'). Falling back to copy shader.", g_ActiveConfig.sScalingShader.c_str());
		OSD::AddMessage(StringFromFormat("Failed to load scaling shader ('%s'). Falling back to copy shader.", g_ActiveConfig.sScalingShader.c_str()));
		m_scaling_config.reset();
	}
}

void PostProcessor::ReloadStereoShaderConfig()
{
	m_stereo_config.reset();
	if (g_ActiveConfig.iStereoMode == STEREO_ENABLED)
	{
		m_stereo_config = std::make_unique<PostProcessingShaderConfiguration>();
		if (!m_stereo_config->LoadShader(STEREO_SHADER_SUBDIR, g_ActiveConfig.sStereoShader))
		{
			ERROR_LOG(VIDEO, "Failed to load scaling shader ('%s'). Falling back to blit.", g_ActiveConfig.sStereoShader.c_str());
			OSD::AddMessage(StringFromFormat("Failed to load scaling shader ('%s'). Falling back to blit.", g_ActiveConfig.sStereoShader.c_str()));
			m_stereo_config.reset();
		}
	}
}

std::string PostProcessor::GetUniformBufferShaderSource(API_TYPE api, const PostProcessingShaderConfiguration* config)
{
	std::string shader_source;

	// Constant block
	if (api == API_OPENGL)
		shader_source += "layout(std140) uniform PostProcessingConstants {\n";
	else if (api == API_D3D)
		shader_source += "cbuffer PostProcessingConstants : register(b0) {\n";

	// Common constants
	shader_source += "\tfloat4 input_resolutions[4];\n";
	shader_source += "\tfloat4 src_rect;\n";
	shader_source += "\tfloat4 target_resolution;\n";
	shader_source += "\tfloat time;\n";
	shader_source += "\tfloat src_layer;\n";
	shader_source += "\tfloat unused0_;\n";
	shader_source += "\tfloat unused1_;\n";		// align to float4

	// User options
	u32 unused_counter = 2;
	for (const auto& it : config->GetOptions())
	{
		if (it.second.m_compile_time_constant)
			continue;

		if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_BOOL)
		{
			shader_source += StringFromFormat("\tint %s;\n", it.first.c_str());
			for (u32 i = 1; i < 4; i++)
				shader_source += StringFromFormat("\tint unused%u_;\n", unused_counter++);
		}
		else if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_INTEGER)
		{
			u32 count = static_cast<u32>(it.second.m_integer_values.size());
			if (count == 1)
				shader_source += StringFromFormat("\tint %s;\n", it.first.c_str());
			else
				shader_source += StringFromFormat("\tint%d %s;\n", count, it.first.c_str());

			for (u32 i = count; i < 4; i++)
				shader_source += StringFromFormat("\tint unused%u_;\n", unused_counter++);
		}
		else if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_FLOAT)
		{
			u32 count = static_cast<u32>(it.second.m_float_values.size());
			if (count == 1)
				shader_source += StringFromFormat("\tfloat %s;\n", it.first.c_str());
			else
				shader_source += StringFromFormat("\tfloat%d %s;\n", count, it.first.c_str());

			for (u32 i = count; i < 4; i++)
				shader_source += StringFromFormat("\tint unused%u_;\n", unused_counter++);
		}
	}

	// End constant block
	shader_source += "};\n";
	return shader_source;
}

std::string PostProcessor::GetPassFragmentShaderSource(API_TYPE api, const PostProcessingShaderConfiguration* config,
													   const PostProcessingShaderConfiguration::RenderPass* pass)
{
	std::string shader_source;
	if (api == API_OPENGL)
	{
		shader_source += "#define API_OPENGL 1\n";
		shader_source += "#define GLSL 1\n";
		shader_source += s_post_fragment_header_ogl;
	}
	else if (api == API_D3D)
	{
		shader_source += "#define API_D3D 1\n";
		shader_source += "#define HLSL 1\n";
		shader_source += s_post_fragment_header_d3d;
	}

	// Add uniform buffer
	shader_source += GetUniformBufferShaderSource(api, config);

	// Figure out which input indices map to color/depth/previous buffers.
	// If any of these buffers is not bound, defaults of zero are fine here,
	// since that buffer may not even be used by the shdaer.
	int color_buffer_index = 0;
	int depth_buffer_index = 0;
	int prev_output_index = 0;
	for (const PostProcessingShaderConfiguration::RenderPass::Input& input : pass->inputs)
	{
		switch (input.type)
		{
		case POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER:
			color_buffer_index = input.texture_unit;
			break;

		case POST_PROCESSING_INPUT_TYPE_DEPTH_BUFFER:
			depth_buffer_index = input.texture_unit;
			break;

		case POST_PROCESSING_INPUT_TYPE_PREVIOUS_PASS_OUTPUT:
			prev_output_index = input.texture_unit;
			break;

		default:
			break;
		}
	}

	// Hook the discovered indices up to macros.
	// This is to support the SampleDepth, SamplePrev, etc. macros.
	shader_source += StringFromFormat("#define COLOR_BUFFER_INPUT_INDEX %d\n", color_buffer_index);
	shader_source += StringFromFormat("#define DEPTH_BUFFER_INPUT_INDEX %d\n", depth_buffer_index);
	shader_source += StringFromFormat("#define PREV_OUTPUT_INPUT_INDEX %d\n", prev_output_index);

	// Add compile-time constants
	for (const auto& it : config->GetOptions())
	{
		if (!it.second.m_compile_time_constant)
			continue;

		if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_BOOL)
		{
			shader_source += StringFromFormat("#define %s (%d)\n", it.first.c_str(), (int)it.second.m_bool_value);
		}
		else if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_INTEGER)
		{
			int count = static_cast<int>(it.second.m_integer_values.size());
			std::string type_str = (count == 1) ? "int" : StringFromFormat("int%d", count);
			shader_source += StringFromFormat("#define %s %s(", it.first.c_str(), type_str.c_str());
			for (int i = 0; i < count; i++)
				shader_source += StringFromInt(it.second.m_integer_values[i]);
			shader_source += ")\n";
		}
		else if (it.second.m_type == POST_PROCESSING_OPTION_TYPE_FLOAT)
		{
			int count = static_cast<int>(it.second.m_float_values.size());
			std::string type_str = (count == 1) ? "float" : StringFromFormat("float%d", count);
			shader_source += StringFromFormat("#define %s %s(", it.first.c_str(), type_str.c_str());
			for (int i = 0; i < count; i++)
				shader_source += StringFromFormat("%f", it.second.m_float_values[i]);
			shader_source += ")\n";
		}
	}

	// Remaining wrapper/interfacing functions
	shader_source += s_post_fragment_header_common;

	// Bit of a hack, but we need to change the name of main temporarily.
	// This can go once the compiler is modified to support different entry points.
	if (api == API_D3D && pass->entry_point == "main")
		shader_source += "#define main real_main\n";

	// Include the user's code here
	if (!pass->entry_point.empty())
	{
		shader_source += config->GetShaderSource();
		shader_source += '\n';
	}

	// API-specific wrapper
	if (api == API_OPENGL)
	{
		// No entry point? This pass should perform a copy.
		if (pass->entry_point.empty())
			shader_source += "void main() { ocol0 = SampleInput(0); }\n";
		else if (pass->entry_point != "main")
			shader_source += StringFromFormat("void main() { %s(); }\n", pass->entry_point.c_str());
	}
	else if (api == API_D3D)
	{
		if (pass->entry_point == "main")
			shader_source += "#undef main\n";

		shader_source += "void main(in float4 in_pos : SV_Position, in float3 in_uv0 : TEXCOORD0, out float4 out_col0 : SV_Target) {\n";
		shader_source += "\tfragcoord = in_pos.xy;\n";
		shader_source += "\tuv0 = in_uv0.xy;\n";
		shader_source += "\tlayer = in_uv0.z;\n";

		// No entry point? This pass should perform a copy.
		if (pass->entry_point.empty())
			shader_source += "\tocol0 = SampleInput(0);\n";
		else
			shader_source += StringFromFormat("\t%s();\n", (pass->entry_point != "main") ? pass->entry_point.c_str() : "real_main");

		shader_source += "\tout_col0 = ocol0;\n";
		shader_source += "}\n";
	}

	return shader_source;
}

TargetSize PostProcessor::ScaleTargetSize(const TargetSize& orig_size, float scale)
{
	TargetSize size;

	// negative scale means scale to native first
	if (scale < 0.0f)
	{
		float native_scale = -scale;
		int native_width = orig_size.width * EFB_WIDTH / g_renderer->GetTargetWidth();
		int native_height = orig_size.height * EFB_HEIGHT / g_renderer->GetTargetHeight();
		size.width = std::max(static_cast<int>(std::round(((float)native_width * native_scale))), 1);
		size.height = std::max(static_cast<int>(std::round(((float)native_height * native_scale))), 1);

	}
	else
	{
		size.width = std::max(static_cast<int>(std::round((float)orig_size.width * scale)), 1);
		size.height = std::max(static_cast<int>(std::round((float)orig_size.height * scale)), 1);
	}

	return size;
}

TargetRectangle PostProcessor::ScaleTargetRectangle(API_TYPE api, const TargetRectangle& src, float scale)
{
	TargetRectangle dst;

	// negative scale means scale to native first
	if (scale < 0.0f)
	{
		float native_scale = -scale;
		int native_left = src.left * EFB_WIDTH / g_renderer->GetTargetWidth();
		int native_right = src.right * EFB_WIDTH / g_renderer->GetTargetWidth();
		int native_top = src.top * EFB_HEIGHT / g_renderer->GetTargetHeight();
		int native_bottom = src.bottom * EFB_HEIGHT / g_renderer->GetTargetHeight();
		dst.left = static_cast<int>(std::round((float)native_left * native_scale));
		dst.right = static_cast<int>(std::round((float)native_right * native_scale));
		dst.top = static_cast<int>(std::round((float)native_top * native_scale));
		dst.bottom = static_cast<int>(std::round((float)native_bottom * native_scale));
	}
	else
	{
		dst.left = static_cast<int>(std::round((float)src.left * scale));
		dst.right = static_cast<int>(std::round((float)src.right * scale));
		dst.top = static_cast<int>(std::round((float)src.top * scale));
		dst.bottom = static_cast<int>(std::round((float)src.bottom * scale));
	}

	// D3D can't handle zero viewports, so protect against this here
	if (api == API_D3D)
	{
		dst.right = std::max(dst.right, dst.left + 1);
		dst.bottom = std::max(dst.bottom, dst.top + 1);
	}

	return dst;
}

const std::string PostProcessor::s_post_fragment_header_ogl = R"(

// Type aliases.
#define float2x2 mat2
#define float3x3 mat3
#define float4x4 mat4
#define float4x3 mat4x3

// Utility functions.
float saturate(float x) { return clamp(x, 0.0f, 1.0f); }
float2 saturate(float2 x) { return clamp(x, float2(0.0f, 0.0f), float2(1.0f, 1.0f)); }
float3 saturate(float3 x) { return clamp(x, float3(0.0f, 0.0f, 0.0f), float3(1.0f, 1.0f, 1.0f)); }
float4 saturate(float4 x) { return clamp(x, float4(0.0f, 0.0f, 0.0f, 0.0f), float4(1.0f, 1.0f, 1.0f, 1.0f)); }

// Flipped multiplication order because GLSL matrices use column vectors.
float2 mul(float2x2 m, float2 v) { return (v * m); }
float3 mul(float3x3 m, float3 v) { return (v * m); }
float4 mul(float4x3 m, float3 v) { return (v * m); }
float4 mul(float4x4 m, float4 v) { return (v * m); }
float2 mul(float2 v, float2x2 m) { return (m * v); }
float3 mul(float3 v, float3x3 m) { return (m * v); }
float3 mul(float4 v, float4x3 m) { return (m * v); }
float4 mul(float4 v, float4x4 m) { return (m * v); }

// Depth value is not inverted for GL
#define DEPTH_VALUE(val) (val)

// Shader inputs/outputs
SAMPLER_BINDING(9) uniform sampler2DArray pp_inputs[4];
in float2 uv0;
flat in float layer;
out float4 ocol0;

// Input sampling wrappers
float4 SampleInput(int index) { return texture(pp_inputs[index], float3(uv0, layer)); }
float4 SampleInputLocation(int index, float2 location) { return texture(pp_inputs[index], float3(location, layer)); }
float4 SampleInputLayer(int index, int slayer) { return texture(pp_inputs[index], float3(uv0, float(slayer))); }
float4 SampleInputLayerLocation(int index, int slayer, float2 location) { return texture(pp_inputs[index], float3(location, float(slayer))); }
float2 GetFragmentCoord() { return gl_FragCoord.xy; }

// Input sampling with offset, macro because offset must be a constant expression.
#define SampleInputOffset(index, offset) (textureOffset(pp_inputs[index], float3(uv0, layer), offset))
#define SampleInputLayerOffset(index, slayer, offset) (textureOffset(pp_inputs[index], float3(uv0, float(slayer)), offset))

)";

const std::string PostProcessor::s_post_fragment_header_d3d = R"(

// Depth value is inverted for D3D
#define DEPTH_VALUE(val) (1.0f - (val))

// Shader inputs
Texture2DArray pp_inputs[4] : register(t9);
SamplerState pp_input_samplers[4] : register(s9);

// Shadows of those read/written in main
static float2 fragcoord;
static float2 uv0;
static float4 ocol0;
static float layer;

// Input sampling wrappers
float4 SampleInput(int index) { return pp_inputs[index].Sample(pp_input_samplers[index], float3(uv0, layer)); }
float4 SampleInputLocation(int index, float2 location) { return pp_inputs[index].Sample(pp_input_samplers[index], float3(location, layer)); }
float4 SampleInputLayer(int index, int slayer) { return pp_inputs[index].Sample(pp_input_samplers[index], float3(uv0, float(slayer))); }
float4 SampleInputLayerLocation(int index, int slayer, float2 location) { return pp_inputs[index].Sample(pp_input_samplers[index], float3(location, float(slayer))); }
float2 GetFragmentCoord() { return fragcoord; }

// Input sampling with offset, macro because offset must be a constant expression.
#define SampleInputOffset(index, offset) (pp_inputs[index].Sample(pp_input_samplers[index], float3(uv0, layer), offset))
#define SampleInputLayerOffset(index, slayer, offset) (pp_inputs[index].Sample(pp_input_samplers[index], float3(uv0, float(slayer)), offset))

)";

const std::string PostProcessor::s_post_fragment_header_common = R"(

// Convert z/w -> linear depth
float ToLinearDepth(float depth)
{
	// TODO: Look at where we can pull better values for this from.
	const float NearZ = 0.001f;
	const float FarZ = 1.0f;
	const float A = (1.0f - (FarZ / NearZ)) / 2.0f;
	const float B = (1.0f + (FarZ / NearZ)) / 2.0f;

	return 1.0f / (A * depth + B);
}

// Input resolution accessors
float2 GetInputResolution(int index) { return input_resolutions[index].xy; }
float2 GetInvInputResolution(int index) { return input_resolutions[index].zw; }
float2 GetTargetResolution() { return target_resolution.xy; }
float2 GetInvTargetResolution() { return target_resolution.zw; }

// Interface wrappers - provided for compatibility.
float4 Sample() { return SampleInput(COLOR_BUFFER_INPUT_INDEX); }
float4 SampleLocation(float2 location) { return SampleInputLocation(COLOR_BUFFER_INPUT_INDEX, location); }
float4 SampleLayer(int layer) { return SampleInputLayer(COLOR_BUFFER_INPUT_INDEX, layer); }
float4 SampleLayerLocation(int layer, float2 location) { return SampleInputLayerLocation(COLOR_BUFFER_INPUT_INDEX, layer, location); }
float4 SamplePrev() { return SampleInput(PREV_OUTPUT_INPUT_INDEX); }
float4 SamplePrevLocation(float2 location) { return SampleInputLocation(PREV_OUTPUT_INPUT_INDEX, location); }
float SampleRawDepth() { return DEPTH_VALUE(SampleInput(DEPTH_BUFFER_INPUT_INDEX).x); }
float SampleRawDepthLocation(float2 location) { return DEPTH_VALUE(SampleInputLocation(DEPTH_BUFFER_INPUT_INDEX, location).x); }
float SampleDepth() { return ToLinearDepth(SampleRawDepth()); }
float SampleDepthLocation(float2 location) { return ToLinearDepth(SampleRawDepthLocation(location)); }

// Offset methods are macros, because the offset must be a constant expression.
#define SampleOffset(offset) (SampleInputOffset(COLOR_BUFFER_INPUT_INDEX, offset))
#define SampleLayerOffset(offset, slayer) (SampleInputLayerOffset(COLOR_BUFFER_INPUT_INDEX, slayer, offset))
#define SamplePrevOffset(offset) (SampleInputOffset(PREV_OUTPUT_INPUT_INDEX, offset))
#define SampleRawDepthOffset(offset) (DEPTH_VALUE(SampleInputOffset(DEPTH_BUFFER_INPUT_INDEX, offset).x))
#define SampleDepthOffset(offset) (ToLinearDepth(SampleRawDepthOffset(offset)))

// Backwards compatibility
float2 GetResolution() { return GetInputResolution(COLOR_BUFFER_INPUT_INDEX); }
float2 GetInvResolution() { return GetInvInputResolution(COLOR_BUFFER_INPUT_INDEX); }

// Variable wrappers
float2 GetCoordinates() { return uv0; }
float GetTime() { return time; }
void SetOutput(float4 color) { ocol0 = color; }

// Option check macro
#define GetOption(x) (x)
#define OptionEnabled(x) ((x) != 0)

)";

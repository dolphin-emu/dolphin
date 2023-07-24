// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/EditorMain.h"

#include <filesystem>
#include <fstream>

#include <fmt/format.h>
#include <imgui.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/System.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Assets/CustomAssetLoader.h"
#include "VideoCommon/Assets/CustomTextureData.h"
#include "VideoCommon/GraphicsModEditor/Controls/MeshExtractWindow.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"
#include "VideoCommon/GraphicsModEditor/EditorState.h"
#include "VideoCommon/GraphicsModEditor/EditorTypes.h"
#include "VideoCommon/GraphicsModEditor/Panels/ActiveTargetsPanel.h"
#include "VideoCommon/GraphicsModEditor/Panels/AssetBrowserPanel.h"
#include "VideoCommon/GraphicsModEditor/Panels/PropertiesPanel.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsMod.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/CustomPipelineAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/ModifyLight.h"
#include "VideoCommon/HiresTextures.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoConfig.h"

namespace GraphicsModEditor
{
namespace
{
bool AddTextureToResources(const std::string& texture_path, const std::string& name,
                           EditorState* state)
{
  VideoCommon::CustomTextureData::ArraySlice::Level level;
  if (!VideoCommon::LoadPNGTexture(&level, texture_path))
  {
    return false;
  }

  TextureConfig tex_config(level.width, level.height, 1, 1, 1, AbstractTextureFormat::RGBA8, 0,
                           AbstractTextureType::Texture_2DArray);
  auto editor_tex = g_gfx->CreateTexture(tex_config, name);
  if (!editor_tex)
  {
    PanicAlertFmt("Failed to create editor texture '{}'", name);
    return false;
  }

  editor_tex->Load(0, level.width, level.height, level.width, level.data.data(),
                   sizeof(u32) * level.width * level.height);
  state->m_editor_data.m_name_to_texture[name] = std::move(editor_tex);

  return true;
}

bool AddTemplate(const std::string& template_path, const std::string& name, EditorState* state)
{
  std::string template_data;
  if (!File::ReadFileToString(template_path, template_data))
  {
    PanicAlertFmt("Failed to load editor template '{}'", name);
    return false;
  }

  state->m_editor_data.m_name_to_template[name] = template_data;
  return true;
}
}  // namespace
EditorMain::EditorMain() = default;
EditorMain::~EditorMain() = default;

bool EditorMain::Initialize()
{
  if (!RebuildState())
    return false;

  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // TODO
  m_enabled = true;

  return true;
}

void EditorMain::Shutdown()
{
  m_active_targets_panel.reset();
  m_asset_browser_panel.reset();
  m_properties_panel.reset();

  m_state.reset();

  m_has_changes = false;
  m_enabled = false;
  m_editor_session_in_progress = false;
}

void EditorMain::DrawImGui()
{
  if (!m_enabled)
    return;

  DrawMenu();

  if (m_editor_session_in_progress)
  {
    m_active_targets_panel->DrawImGui();

    if (!m_inspect_only)
      m_asset_browser_panel->DrawImGui();
    m_properties_panel->DrawImGui();
  }
  else
  {
    // ImGui::ShowDemoWindow();
  }
}

void EditorMain::AddDrawCall(DrawCallData draw_call)
{
  m_active_targets_panel->AddDrawCall(std::move(draw_call));
}

void EditorMain::AddFBCall(FBCallData fb_call)
{
  m_active_targets_panel->AddFBCall(std::move(fb_call));
}

void EditorMain::AddLightData(LightData light_data)
{
  m_active_targets_panel->AddLightData(std::move(light_data));
}

const std::vector<GraphicsModAction*>&
EditorMain::GetProjectionActions(ProjectionType projection_type) const
{
  return m_empty_actions;
}

const std::vector<GraphicsModAction*>&
EditorMain::GetProjectionTextureActions(ProjectionType projection_type,
                                        const std::string& texture_name) const
{
  return m_empty_actions;
}

const std::vector<GraphicsModAction*>&
EditorMain::GetDrawStartedActions(GraphicsMods::DrawCallID draw_call_id) const
{
  const OperationAndDrawCallID::Operation operation = OperationAndDrawCallID::Operation::Draw;
  OperationAndDrawCallID lookup{operation, draw_call_id};

  if (const auto it = m_state->m_editor_data.m_operation_and_draw_call_id_to_actions.find(lookup);
      it != m_state->m_editor_data.m_operation_and_draw_call_id_to_actions.end())
  {
    return it->second;
  }

  if (const auto it = m_state->m_user_data.m_operation_and_draw_call_id_to_actions.find(lookup);
      it != m_state->m_user_data.m_operation_and_draw_call_id_to_actions.end())
  {
    return it->second;
  }

  return m_empty_actions;
}

const std::vector<GraphicsModAction*>&
EditorMain::GetTextureLoadActions(const std::string& texture_name) const
{
  // TODO

  return m_empty_actions;
}

const std::vector<GraphicsModAction*>&
EditorMain::GetTextureCreateActions(const std::string& texture_name) const
{
  // TODO

  return m_empty_actions;
}

const std::vector<GraphicsModAction*>& EditorMain::GetEFBActions(const FBInfo& fb) const
{
  if (const auto it = m_state->m_editor_data.m_fb_call_id_to_actions.find(fb);
      it != m_state->m_editor_data.m_fb_call_id_to_actions.end())
  {
    return it->second;
  }

  if (const auto it = m_state->m_user_data.m_fb_call_id_to_reference_actions.find(fb);
      it != m_state->m_user_data.m_fb_call_id_to_reference_actions.end())
  {
    return it->second;
  }

  return m_empty_actions;
}

const std::vector<GraphicsModAction*>&
EditorMain::GetLightActions(GraphicsMods::LightID light_id) const
{
  if (const auto it = m_state->m_editor_data.m_light_id_to_actions.find(light_id);
      it != m_state->m_editor_data.m_light_id_to_actions.end())
  {
    return it->second;
  }

  if (const auto it = m_state->m_user_data.m_light_id_to_reference_actions.find(light_id);
      it != m_state->m_user_data.m_light_id_to_reference_actions.end())
  {
    return it->second;
  }

  return m_empty_actions;
}

bool EditorMain::RebuildState()
{
  m_state = std::make_unique<EditorState>();
  m_active_targets_panel = std::make_unique<Panels::ActiveTargetsPanel>(*m_state);
  m_asset_browser_panel = std::make_unique<Panels::AssetBrowserPanel>(*m_state);
  m_properties_panel = std::make_unique<Panels::PropertiesPanel>(*m_state);

  m_state->m_user_data.m_asset_library = std::make_shared<EditorAssetSource>();
  m_state->m_editor_data.m_asset_library =
      std::make_shared<VideoCommon::DirectFilesystemAssetLibrary>();
  m_change_occurred_event =
      EditorEvents::ChangeOccurredEvent::Register([this] { OnChangeOccured(); }, "EditorMain");

  const std::string textures_path_root =
      File::GetSysDirectory() + GRAPHICSMODEDITOR_DIR + "/Textures";
  if (!AddTextureToResources(fmt::format("{}/icons8-portraits-50.png", textures_path_root),
                             "hollow_cube", m_state.get()))
  {
    return false;
  }
  if (!AddTextureToResources(fmt::format("{}/icons8-document-500.png", textures_path_root), "file",
                             m_state.get()))
  {
    return false;
  }
  if (!AddTextureToResources(fmt::format("{}/icons8-folder-50.png", textures_path_root),
                             "tiny_folder", m_state.get()))
  {
    return false;
  }
  if (!AddTextureToResources(fmt::format("{}/icons8-folder-500.png", textures_path_root), "folder",
                             m_state.get()))
  {
    return false;
  }
  if (!AddTextureToResources(fmt::format("{}/icons8-image-file-500.png", textures_path_root),
                             "image", m_state.get()))
  {
    return false;
  }
  if (!AddTextureToResources(fmt::format("{}/icons8-code-file-100.png", textures_path_root), "code",
                             m_state.get()))
  {
    return false;
  }
  if (!AddTextureToResources(fmt::format("{}/icons8-cube-filled-50.png", textures_path_root),
                             "filled_cube", m_state.get()))
  {
    return false;
  }
  if (!AddTextureToResources(fmt::format("{}/icons8-search-50.png", textures_path_root), "search",
                             m_state.get()))
  {
    return false;
  }
  if (!AddTextureToResources(fmt::format("{}/icons8-error-50.png", textures_path_root), "error",
                             m_state.get()))
  {
    return false;
  }

  const std::string templates_path_root =
      File::GetSysDirectory() + GRAPHICSMODEDITOR_DIR + "/Templates";
  if (!AddTemplate(fmt::format("{}/material.json", templates_path_root), "material", m_state.get()))
  {
    return false;
  }
  if (!AddTemplate(fmt::format("{}/pixel_shader.json", templates_path_root),
                   "pixel_shader_metadata", m_state.get()))
  {
    return false;
  }
  if (!AddTemplate(fmt::format("{}/pixel_shader.glsl", templates_path_root), "pixel_shader",
                   m_state.get()))
  {
    return false;
  }

  const std::string pipeline_path_root =
      File::GetSysDirectory() + GRAPHICSMODEDITOR_DIR + "/Pipelines";
  const std::filesystem::path pipeline_path_root_fs{pipeline_path_root};

  VideoCommon::DirectFilesystemAssetLibrary::AssetMap shader_asset_map;
  shader_asset_map.try_emplace("metadata",
                               pipeline_path_root_fs / "highlight" / "color.shader.json");
  shader_asset_map.try_emplace("shader", pipeline_path_root_fs / "highlight" / "color.glsl");
  m_state->m_editor_data.m_asset_library->SetAssetIDMapData("highlight_shader",
                                                            std::move(shader_asset_map));

  VideoCommon::DirectFilesystemAssetLibrary::AssetMap material_asset_map;
  material_asset_map.try_emplace("", pipeline_path_root_fs / "highlight" / "material.json");
  m_state->m_editor_data.m_asset_library->SetAssetIDMapData("highlight_material",
                                                            std::move(material_asset_map));

  auto& system = Core::System::GetInstance();
  auto& asset_loader = system.GetCustomAssetLoader();
  asset_loader.Reset();

  m_state->m_editor_data.m_assets.push_back(
      asset_loader.LoadPixelShader("highlight_shader", m_state->m_editor_data.m_asset_library));
  m_state->m_editor_data.m_assets.push_back(
      asset_loader.LoadMaterial("highlight_material", m_state->m_editor_data.m_asset_library));

  std::vector<CustomPipelineAction::PipelinePassPassDescription> passes;
  passes.emplace_back().m_pixel_material_asset = "highlight_material";

  m_state->m_editor_data.m_highlight_action = std::make_unique<CustomPipelineAction>(
      m_state->m_editor_data.m_asset_library, std::move(passes));

  m_state->m_editor_data.m_highlight_light_action = std::make_unique<ModifyLightAction>(
      float4{0, 0, 1, 0}, std::nullopt, std::nullopt, std::nullopt, std::nullopt);

  return true;
}

void EditorMain::DrawMenu()
{
  bool new_mod_popup = false;
  if (ImGui::BeginMainMenuBar())
  {
    if (ImGui::BeginMenu("File"))
    {
      if (ImGui::BeginMenu("New"))
      {
        if (ImGui::MenuItem("Project"))
        {
          new_mod_popup = true;
        }
        if (ImGui::MenuItem("Inspect Only"))
        {
          m_editor_session_in_progress = true;
          m_inspect_only = true;
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Open"))
      {
        const std::string& game_id = SConfig::GetInstance().GetGameID();
        const auto directories =
            GetTextureDirectoriesWithGameId(File::GetUserPath(D_GRAPHICSMOD_IDX), game_id);
        if (directories.empty())
        {
          ImGui::Text("No available projects, create a new project instead");
        }
        else
        {
          for (const auto& directory : directories)
          {
            const auto name = StringToPath(directory).filename();
            const auto name_str = PathToString(name);
            if (ImGui::MenuItem(name_str.c_str()))
            {
              LoadMod(name_str);
            }
          }
        }
        ImGui::EndMenu();
      }
      if (ImGui::MenuItem("Save", "Ctrl+S", false,
                          m_has_changes && m_editor_session_in_progress && !m_inspect_only))
      {
        Save();
      }
      if (ImGui::MenuItem("Save As..", nullptr, false, false))
      {
        // TODO
      }
      if (ImGui::MenuItem("Close", nullptr, false, m_editor_session_in_progress))
      {
        Close();
      }

      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Scene"))
    {
      if (ImGui::MenuItem("Export Scene As Mesh", nullptr, false, m_editor_session_in_progress))
      {
        if (!m_open_mesh_dump_export_window)
        {
          for (const auto& [draw_call_id, data] : m_state->m_runtime_data.m_draw_call_id_to_data)
          {
            m_last_mesh_dump_request.m_draw_call_ids.insert(draw_call_id);
          }
        }
        m_open_mesh_dump_export_window = true;
      }
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }

  if (m_open_mesh_dump_export_window && m_state)
  {
    if (Controls::ShowMeshExtractWindow(m_state->m_scene_dumper, m_last_mesh_dump_request))
    {
      m_open_mesh_dump_export_window = false;
      m_last_mesh_dump_request = {};
    }
  }

  const std::string_view new_graphics_mod_popup_name = "New Graphics Mod";
  if (new_mod_popup)
  {
    if (!ImGui::IsPopupOpen(new_graphics_mod_popup_name.data()))
    {
      m_editor_new_mod_name = "";
      m_editor_new_mod_author = "";
      m_editor_new_mod_description = "";
      ImGui::OpenPopup(new_graphics_mod_popup_name.data());
    }
  }

  // New mod popup below
  const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal(new_graphics_mod_popup_name.data(), nullptr))
  {
    bool is_valid = false;

    if (ImGui::BeginTable("NewModForm", 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Name");
      ImGui::TableNextColumn();
      ImGui::InputText("##NewModName", &m_editor_new_mod_name);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Author");
      ImGui::TableNextColumn();
      ImGui::InputText("##NewModAuthor", &m_editor_new_mod_author);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Description");
      ImGui::TableNextColumn();
      ImGui::InputTextMultiline("##NewModDescription", &m_editor_new_mod_description);
      ImGui::EndTable();
    }

    const std::string& graphics_mod_root = File::GetUserPath(D_GRAPHICSMOD_IDX);
    if (!std::filesystem::exists(std::filesystem::path{graphics_mod_root} / m_editor_new_mod_name))
    {
      is_valid = true;
    }

    if (!is_valid)
      ImGui::BeginDisabled();
    if (ImGui::Button("Create", ImVec2(120, 0)))
    {
      NewMod(m_editor_new_mod_name, m_editor_new_mod_author, m_editor_new_mod_description);
      ImGui::CloseCurrentPopup();
    }
    if (!is_valid)
      ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0)))
    {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

bool EditorMain::NewMod(std::string_view name, std::string_view author,
                        std::string_view description)
{
  const std::string& graphics_mod_root = File::GetUserPath(D_GRAPHICSMOD_IDX);
  const std::filesystem::path mod_path = std::filesystem::path{graphics_mod_root} / name;
  if (std::filesystem::exists(mod_path))
  {
    // TODO: error
    return false;
  }

  std::error_code error_code;
  std::filesystem::create_directory(mod_path, error_code);
  if (error_code)
  {
    // TODO: error
    return false;
  }

  const std::string& game_id = SConfig::GetInstance().GetGameID();
  if (!File::WriteStringToFile(PathToString(mod_path / fmt::format("{}.txt", game_id)), ""))
    return false;

  if (!RebuildState())
    return false;

  m_state->m_user_data.m_title = name;
  m_state->m_user_data.m_author = author;
  m_state->m_user_data.m_description = description;
  m_state->m_user_data.m_current_mod_path = mod_path;
  m_has_changes = false;
  m_editor_session_in_progress = true;
  m_inspect_only = false;

  m_asset_browser_panel->ResetCurrentPath();

  return true;
}

bool EditorMain::LoadMod(std::string_view name)
{
  const std::string& graphics_mod_root = File::GetUserPath(D_GRAPHICSMOD_IDX);
  const std::filesystem::path mod_path = std::filesystem::path{graphics_mod_root} / name;
  if (!std::filesystem::exists(mod_path))
  {
    // TODO: error
    return false;
  }

  if (!RebuildState())
    return false;

  const auto config = GraphicsModConfig::Create(PathToString(mod_path / "metadata.json"),
                                                GraphicsModConfig::Source::User);
  if (!config)
  {
    // TODO: error
    return false;
  }

  ReadFromGraphicsMod(&m_state->m_user_data, *config);

  auto& system = Core::System::GetInstance();
  auto& loader = system.GetCustomAssetLoader();
  for (const auto& asset : config->m_assets)
  {
    // TODO: generate preview for other types?
    if (asset.m_map.find("texture") != asset.m_map.end())
    {
      m_state->m_editor_data.m_assets_waiting_for_preview.try_emplace(
          asset.m_asset_id,
          loader.LoadGameTexture(asset.m_asset_id, m_state->m_user_data.m_asset_library));
    }
  }

  m_state->m_user_data.m_current_mod_path = mod_path;
  m_has_changes = false;
  m_editor_session_in_progress = true;
  m_inspect_only = false;

  m_asset_browser_panel->ResetCurrentPath();

  return true;
}

void EditorMain::Save()
{
  if (!m_has_changes)
    return;

  const std::string file_path =
      PathToString(m_state->m_user_data.m_current_mod_path / "metadata.json");
  std::ofstream json_stream;
  File::OpenFStream(json_stream, file_path, std::ios_base::out);
  if (!json_stream.is_open())
  {
    ERROR_LOG_FMT(VIDEO, "Failed to open graphics mod json file '{}' for writing", file_path);
    return;
  }

  m_state->m_user_data.m_asset_library->SaveAssetDataAsFiles();

  GraphicsModConfig mod;
  WriteToGraphicsMod(m_state->m_user_data, &mod);

  picojson::object serialized_root;
  mod.SerializeToConfig(serialized_root);

  const auto output = picojson::value{serialized_root}.serialize(true);
  json_stream << output;

  m_has_changes = false;
}

void EditorMain::Close()
{
  if (m_has_changes)
  {
    // Ask "are you sure?"
  }

  m_editor_session_in_progress = false;
  m_inspect_only = false;
}

void EditorMain::OnChangeOccured()
{
  m_has_changes = true;
}

EditorState* EditorMain::GetEditorState() const
{
  return m_state.get();
}

SceneDumper* EditorMain::GetSceneDumper() const
{
  if (m_state) [[likely]]
  {
    return &m_state->m_scene_dumper;
  }

  return nullptr;
}
}  // namespace GraphicsModEditor

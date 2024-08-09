// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/EditorMain.h"

#include <filesystem>
#include <fstream>

#include <fmt/format.h>
#include <imgui.h>

#include "Common/CommonPaths.h"
#include "Common/EnumUtils.h"
#include "Common/FileUtil.h"
#include "Common/JsonUtil.h"
#include "Core/ConfigManager.h"
#include "Core/System.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Assets/CustomAssetLoader.h"
#include "VideoCommon/Assets/CustomTextureData.h"
#include "VideoCommon/GraphicsModEditor/Controls/MeshExtractWindow.h"
#include "VideoCommon/GraphicsModEditor/EditorBackend.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"
#include "VideoCommon/GraphicsModEditor/EditorState.h"
#include "VideoCommon/GraphicsModEditor/EditorTypes.h"
#include "VideoCommon/GraphicsModEditor/Panels/ActiveTargetsPanel.h"
#include "VideoCommon/GraphicsModEditor/Panels/AssetBrowserPanel.h"
#include "VideoCommon/GraphicsModEditor/Panels/PropertiesPanel.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsMod.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/CustomPipelineAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/ModifyLight.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CustomTextureCache.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModManager.h"
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

  auto& system = Core::System::GetInstance();
  system.GetGraphicsModManager().SetEditorBackend(nullptr);

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

bool EditorMain::RebuildState()
{
  m_state = std::make_unique<EditorState>();
  m_active_targets_panel = std::make_unique<Panels::ActiveTargetsPanel>(*m_state);
  m_asset_browser_panel = std::make_unique<Panels::AssetBrowserPanel>(*m_state);
  m_properties_panel = std::make_unique<Panels::PropertiesPanel>(*m_state);

  m_state->m_user_data.m_asset_library = std::make_shared<EditorAssetSource>();
  m_state->m_editor_data.m_asset_library =
      std::make_shared<VideoCommon::DirectFilesystemAssetLibrary>();
  m_state->m_editor_data.m_asset_library->Watch(File::GetSysDirectory() + GRAPHICSMODEDITOR_DIR);
  m_state->m_runtime_data.m_texture_cache = std::make_shared<VideoCommon::CustomTextureCache>();
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

  auto& system = Core::System::GetInstance();
  auto& asset_loader = system.GetCustomAssetLoader();
  asset_loader.Reset();

  m_asset_reload_event = EditorEvents::AssetReloadEvent::Register(
      [&asset_loader, this](const VideoCommon::CustomAssetLibrary::AssetID& asset_id) {
        asset_loader.ReloadAsset(asset_id);
        OnChangeOccured();
      },
      "EditorMain");

  const std::string pipeline_path_root =
      File::GetSysDirectory() + GRAPHICSMODEDITOR_DIR + "/Pipelines";
  const std::filesystem::path pipeline_path_root_fs{pipeline_path_root};

  {
    VideoCommon::Assets::AssetMap highlight_shader_asset_map;
    highlight_shader_asset_map.try_emplace("metadata", pipeline_path_root_fs / "highlight" /
                                                           "color.shader.json");
    highlight_shader_asset_map.try_emplace("shader",
                                           pipeline_path_root_fs / "highlight" / "color.glsl");
    m_state->m_editor_data.m_asset_library->SetAssetIDMapData(
        "highlight_shader", std::move(highlight_shader_asset_map));

    VideoCommon::Assets::AssetMap highlight_material_asset_map;
    highlight_material_asset_map.try_emplace("",
                                             pipeline_path_root_fs / "highlight" / "material.json");
    m_state->m_editor_data.m_asset_library->SetAssetIDMapData(
        "highlight_material", std::move(highlight_material_asset_map));

    m_state->m_editor_data.m_assets.push_back(
        asset_loader.LoadPixelShader("highlight_shader", m_state->m_editor_data.m_asset_library));
    m_state->m_editor_data.m_assets.push_back(
        asset_loader.LoadMaterial("highlight_material", m_state->m_editor_data.m_asset_library));

    std::vector<CustomPipelineAction::PipelinePassPassDescription> passes;
    passes.emplace_back().m_pixel_material_asset = "highlight_material";

    m_state->m_editor_data.m_highlight_action = std::make_unique<CustomPipelineAction>(
        m_state->m_editor_data.m_asset_library, m_state->m_runtime_data.m_texture_cache,
        std::move(passes));
  }

  {
    VideoCommon::Assets::AssetMap simple_light_visual_shader_asset_map;
    simple_light_visual_shader_asset_map.try_emplace("metadata",
                                                     pipeline_path_root_fs / "light_visualization" /
                                                         "simple-light-visualization.shader");
    simple_light_visual_shader_asset_map.try_emplace("shader",
                                                     pipeline_path_root_fs / "light_visualization" /
                                                         "simple-light-visualization.glsl");
    m_state->m_editor_data.m_asset_library->SetAssetIDMapData(
        "simple_light_visualization_shader", std::move(simple_light_visual_shader_asset_map));

    VideoCommon::Assets::AssetMap simple_light_visual_material_asset_map;
    simple_light_visual_material_asset_map.try_emplace(
        "", pipeline_path_root_fs / "light_visualization" / "simple-light-visualization.material");
    m_state->m_editor_data.m_asset_library->SetAssetIDMapData(
        "simple_light_visualization_material", std::move(simple_light_visual_material_asset_map));

    m_state->m_editor_data.m_assets.push_back(asset_loader.LoadPixelShader(
        "simple_light_visualization_shader", m_state->m_editor_data.m_asset_library));
    m_state->m_editor_data.m_assets.push_back(asset_loader.LoadMaterial(
        "simple_light_visualization_material", m_state->m_editor_data.m_asset_library));

    std::vector<CustomPipelineAction::PipelinePassPassDescription> passes;
    passes.emplace_back().m_pixel_material_asset = "simple_light_visualization_material";

    m_state->m_editor_data.m_simple_light_visualization_action =
        std::make_unique<CustomPipelineAction>(m_state->m_editor_data.m_asset_library,
                                               m_state->m_runtime_data.m_texture_cache,
                                               std::move(passes));
  }

  m_state->m_editor_data.m_highlight_light_action = std::make_unique<ModifyLightAction>(
      float4{0, 0, 1, 0}, std::nullopt, std::nullopt, std::nullopt, std::nullopt);

  {
    GraphicsModSystem::Config::GraphicsModTag tag;
    tag.m_name = "Bloom";
    tag.m_description = "Post processing effect that blurs out the brightest areas of the screen";
    tag.m_color = Common::Vec3{0.66f, 0.63f, 0.16f};
    m_state->m_editor_data.m_tags[tag.m_name] = tag;
  }

  {
    GraphicsModSystem::Config::GraphicsModTag tag;
    tag.m_name = "Depth of Field";
    tag.m_description = "Post processing effect that blurs distant objects";
    tag.m_color = Common::Vec3{0.8f, 0, 0};
    m_state->m_editor_data.m_tags[tag.m_name] = tag;
  }

  {
    GraphicsModSystem::Config::GraphicsModTag tag;
    tag.m_name = "User Interface";
    tag.m_description = "";
    tag.m_color = Common::Vec3{0.01f, 0.04f, 0.99f};
    m_state->m_editor_data.m_tags[tag.m_name] = tag;
  }

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

          RebuildState();
          auto& system = Core::System::GetInstance();
          system.GetGraphicsModManager().SetEditorBackend(
              std::make_unique<EditorBackend>(*m_state));
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
        m_open_mesh_dump_export_window = true;
      }
      if (ImGui::MenuItem("Export Scene Metadata As JSON", nullptr, false,
                          m_editor_session_in_progress))
      {
        picojson::array data_objects;
        for (const auto& [draw_call_id, data] : m_state->m_runtime_data.m_draw_call_id_to_data)
        {
          picojson::object obj;
          obj.emplace("draw_call_id", fmt::to_string(Common::ToUnderlying(draw_call_id)));
          obj.emplace("projection type", fmt::to_string(data.draw_data.projection_type));
          // TODO: blending, depth, rasterization...

          picojson::array textures;
          for (const auto& texture : data.draw_data.textures)
          {
            picojson::object texture_obj;
            texture_obj.emplace("hash", texture.hash_name);
            if (texture.texture_type == GraphicsModSystem::TextureType::Normal)
            {
              texture_obj.emplace("type", "Normal");
            }
            else if (texture.texture_type == GraphicsModSystem::TextureType::EFB)
            {
              texture_obj.emplace("type", "EFB");
            }
            else if (texture.texture_type == GraphicsModSystem::TextureType::XFB)
            {
              texture_obj.emplace("type", "XFB");
            }

            textures.push_back(picojson::value{texture_obj});
          }
          obj.emplace("textures", picojson::value{textures});
          data_objects.push_back(picojson::value{obj});
        }
        const std::string path = File::GetUserPath(D_DUMP_IDX) + SConfig::GetInstance().GetGameID();
        JsonToFile(PathToString(path + "_SceneMetadata.json"), picojson::value{data_objects}, true);
      }
      if (ImGui::MenuItem("Disable all actions", nullptr,
                          &m_state->m_editor_data.m_disable_all_actions))
      {
      }
      if (ImGui::MenuItem("View lighting", nullptr, &m_state->m_editor_data.m_view_lighting))
      {
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
  m_state->m_user_data.m_asset_library->Watch(PathToString(mod_path));

  auto& system = Core::System::GetInstance();
  system.GetGraphicsModManager().SetEditorBackend(std::make_unique<EditorBackend>(*m_state));

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

  const auto config =
      GraphicsModSystem::Config::GraphicsMod::Create(PathToString(mod_path / "metadata.json"));
  if (!config)
  {
    // TODO: error
    return false;
  }

  ReadFromGraphicsMod(&m_state->m_user_data, &m_state->m_editor_data, m_state->m_runtime_data,
                      *config, PathToString(mod_path));

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

  m_has_changes = false;
  m_editor_session_in_progress = true;
  m_inspect_only = false;

  m_asset_browser_panel->ResetCurrentPath();
  m_state->m_user_data.m_asset_library->Watch(PathToString(mod_path));

  system.GetGraphicsModManager().SetEditorBackend(std::make_unique<EditorBackend>(*m_state));

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

  GraphicsModSystem::Config::GraphicsMod mod;
  WriteToGraphicsMod(m_state->m_user_data, &mod);

  picojson::object serialized_root;
  mod.Serialize(serialized_root);

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

  auto& system = Core::System::GetInstance();
  system.GetGraphicsModManager().SetEditorBackend(nullptr);
}

void EditorMain::OnChangeOccured()
{
  m_has_changes = true;
}

EditorState* EditorMain::GetEditorState() const
{
  return m_state.get();
}
}  // namespace GraphicsModEditor

// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Based on: https://github.com/encounter/cwdemangle
// Copyright 2024 Luke Street <luke@street.dev>
// SPDX-License-Identifier: CC0-1.0

#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <tuple>

#include "Common/CWDemangler.h"

using namespace CWDemangler;

void DoDemangleTemplateArgsTest(std::string mangled, std::string name, std::string template_args)
{
  DemangleOptions options = DemangleOptions();

  auto result = demangle_template_args(mangled, options);
  DemangleTemplateArgsResult expected = {name, template_args};

  EXPECT_TRUE(result.has_value());
  if (result.has_value())
  {
    EXPECT_EQ(result.value().args, expected.args);
    EXPECT_EQ(result.value().name, expected.name);
  }
}

void DoDemangleNameTest(std::string mangled, std::string name, std::string full_name)
{
  DemangleOptions options = DemangleOptions();

  auto result = demangle_name(mangled, options);
  DemangleNameResult expected = {name, full_name, ""};

  EXPECT_TRUE(result.has_value());
  if (result.has_value())
  {
    EXPECT_EQ(result.value().class_name, expected.class_name);
    EXPECT_EQ(result.value().full, expected.full);
    EXPECT_EQ(result.value().rest, expected.rest);
  }
}

void DoDemangleQualifiedNameTest(std::string mangled, std::string base_name, std::string full_name)
{
  DemangleOptions options = DemangleOptions();

  auto result = demangle_qualified_name(mangled, options);
  DemangleNameResult expected = {base_name, full_name, ""};

  EXPECT_TRUE(result.has_value());
  if (result.has_value())
  {
    EXPECT_EQ(result.value().class_name, expected.class_name);
    EXPECT_EQ(result.value().full, expected.full);
    EXPECT_EQ(result.value().rest, expected.rest);
  }
}

void DoDemangleArgTest(std::string mangled, std::string type_pre, std::string type_post,
                       std::string remainder)
{
  DemangleOptions options = DemangleOptions();

  auto result = CWDemangler::demangle_arg(mangled, options);
  DemangleArgResult expected = {type_pre, type_post, remainder};

  EXPECT_TRUE(result.has_value());
  if (result.has_value())
  {
    EXPECT_EQ(result.value().arg_post, expected.arg_post);
    EXPECT_EQ(result.value().arg_pre, expected.arg_pre);
    EXPECT_EQ(result.value().rest, expected.rest);
  }
}

void DoDemangleFunctionArgsTest(std::string mangled, std::string args, std::string remainder)
{
  DemangleOptions options = DemangleOptions();

  auto result = demangle_function_args(mangled, options);
  DemangleFunctionArgsResult expected = {args, remainder};

  EXPECT_TRUE(result.has_value());
  if (result.has_value())
  {
    EXPECT_EQ(result.value().args, expected.args);
    EXPECT_EQ(result.value().rest, expected.rest);
  }
}

void DoDemangleTest(std::string mangled, std::string demangled)
{
  DemangleOptions options = DemangleOptions();

  auto result = demangle(mangled, options);
  std::optional<std::string> expected = {demangled};
  if (demangled == "")
    expected = std::nullopt;

  EXPECT_EQ(result, expected);
}

void DoDemangleOptionsTest(bool omit_empty_params, bool mw_extensions, std::string mangled,
                           std::string demangled)
{
  DemangleOptions options = DemangleOptions(omit_empty_params, mw_extensions);

  auto result = demangle(mangled, options);
  std::optional<std::string> expected = {demangled};

  EXPECT_EQ(result, expected);
}

TEST(CWDemangler, TestDemangleTemplateArgs)
{
  DoDemangleTemplateArgsTest("ShaderUid<24geometry_shader_uid_data>", "ShaderUid",
                             "<geometry_shader_uid_data>");
  DoDemangleTemplateArgsTest("basic_string<w,Q23std14char_traits<w>,Q23std12allocator<w>>",
                             "basic_string",
                             "<wchar_t, std::char_traits<wchar_t>, std::allocator<wchar_t>>");
}

TEST(CWDemangler, TestDemangleName)
{
  DoDemangleNameTest("37ShaderUid<24geometry_shader_uid_data>", "ShaderUid",
                     "ShaderUid<geometry_shader_uid_data>");
  DoDemangleNameTest("59basic_string<w,Q23std14char_traits<w>,Q23std12allocator<w>>",
                     "basic_string",
                     "basic_string<wchar_t, std::char_traits<wchar_t>, std::allocator<wchar_t>>");
}

TEST(CWDemangler, TestDemangleQualifiedName)
{
  DoDemangleQualifiedNameTest("7Wiimote", "Wiimote", "Wiimote");
  DoDemangleQualifiedNameTest("Q23Gen8XEmitter", "XEmitter", "Gen::XEmitter");
  DoDemangleQualifiedNameTest(
      "Q23std59basic_string<w,Q23std14char_traits<w>,Q23std12allocator<w>>", "basic_string",
      "std::basic_string<wchar_t, std::char_traits<wchar_t>, std::allocator<wchar_t>>");
}

TEST(CWDemangler, TestDemangleArg)
{
  DoDemangleArgTest("v", "void", "", "");
  DoDemangleArgTest("b", "bool", "", "");
  DoDemangleArgTest("RC9CVector3fUc", "const CVector3f&", "", "Uc");
  DoDemangleArgTest("Q23std14char_traits<w>,", "std::char_traits<wchar_t>", "", ",");
  DoDemangleArgTest("PFPCcPCc_v", "void (*", ")(const char*, const char*)", "");
  DoDemangleArgTest("RCPCVPCVUi", "const volatile unsigned int* const volatile* const&", "", "");
}

TEST(CWDemangler, TestDemangleFunctionArgs)
{
  DoDemangleFunctionArgsTest("v", "void", "");
  DoDemangleFunctionArgsTest("b", "bool", "");
  DoDemangleFunctionArgsTest("RC9CVector3fUc_x", "const CVector3f&, unsigned char", "_x");
}

TEST(CWDemangler, TestDemangle)
{
  DoDemangleTest("__ct__11AbstractGfxFv", "AbstractGfx::AbstractGfx()");

  DoDemangleTest("__dt__11AbstractGfxFv", "AbstractGfx::~AbstractGfx()");

  DoDemangleTest(
      "OnPrimitiveCommand__Q216@unnamed@code_c@20FifoPlaybackAnalyzerFQ316@unnamed@code_c@"
      "13OpcodeDecoder9PrimitiveUcUlUsPUc",
      "@unnamed@code_c@::FifoPlaybackAnalyzer::OnPrimitiveCommand(@unnamed@code_c@::OpcodeDecoder::"
      "Primitive, unsigned char, unsigned long, unsigned short, unsigned char*)");

  DoDemangleTest("SetSIMDMode__Q26Common3FPUFQ36Common3FPU9RoundModeb",
                 "Common::FPU::SetSIMDMode(Common::FPU::RoundMode, bool)");

  DoDemangleTest("SupportsUtilityDrawing__11AbstractGfxCFv",
                 "AbstractGfx::SupportsUtilityDrawing() const");

  DoDemangleTest("SetViewport__11AbstractGfxFffffff",
                 "AbstractGfx::SetViewport(float, float, float, float, float, float)");

  DoDemangleTest("CreateTexture__11AbstractGfxFR13TextureConfigQ23std43basic_string_view<c,"
                 "Q23std14char_traits<c>>",
                 "AbstractGfx::CreateTexture(TextureConfig&, std::basic_string_view<char, "
                 "std::char_traits<char>>)");

  DoDemangleTest("SetViewportAndScissor__11AbstractGfxFRQ28MathUtil12Rectangle<i>ff",
                 "AbstractGfx::SetViewportAndScissor(MathUtil::Rectangle<int>&, float, float)");

  DoDemangleTest("GetInstance__18AchievementManagerFv", "AchievementManager::GetInstance()");

  DoDemangleTest("FilterApprovedPatches__18AchievementManagerCFRQ23std70vector<"
                 "Q211PatchEngine5Patch,Q23std32allocator<Q211PatchEngine5Patch>>RQ23std59basic_"
                 "string<c,Q23std14char_traits<c>,Q23std12allocator<c>>Us",
                 "AchievementManager::FilterApprovedPatches(std::vector<PatchEngine::Patch, "
                 "std::allocator<PatchEngine::Patch>>&, std::basic_string<char, "
                 "std::char_traits<char>, std::allocator<char>>&, unsigned short) const");

  DoDemangleTest(
      "__as__Q218AchievementManager15FilereaderStateFPQ218AchievementManager15FilereaderState",
      "AchievementManager::FilereaderState::operator=(AchievementManager::FilereaderState*)");

  DoDemangleTest("LeaderboardEntriesCallback__18AchievementManagerFiPcP34rc_client_leaderboard_"
                 "entry_list_tP11rc_client_tPv",
                 "AchievementManager::LeaderboardEntriesCallback(int, char*, "
                 "rc_client_leaderboard_entry_list_t*, rc_client_t*, void*)");

  DoDemangleTest("Request__18AchievementManagerFP16rc_api_request_tPFP24rc_api_server_response_tPv_"
                 "vPvP11rc_client_t",
                 "AchievementManager::Request(rc_api_request_t*, void "
                 "(*)(rc_api_server_response_t*, void*), void*, rc_client_t*)");

  DoDemangleTest("MemoryPeeker__18AchievementManagerFUlPUcUlP11rc_client_t",
                 "AchievementManager::MemoryPeeker(unsigned long, unsigned char*, unsigned long, "
                 "rc_client_t*)");

  DoDemangleTest("Write__11BoundingBoxFUlQ23std11span<Ci,-1>",
                 "BoundingBox::Write(unsigned long, std::span<const int, -1>)");

  DoDemangleTest(
      "__ct__24CachedInterpreterEmitterFPUcPUc",
      "CachedInterpreterEmitter::CachedInterpreterEmitter(unsigned char*, unsigned char*)");

  DoDemangleTest("Write__24CachedInterpreterEmitterFPFRQ27PowerPC12PowerPCStatePv_iPvUx",
                 "CachedInterpreterEmitter::Write(int (*)(PowerPC::PowerPCState&, void*), void*, "
                 "unsigned long long)");

  DoDemangleTest("Shear__Q26Common8Matrix44Fff", "Common::Matrix44::Shear(float, float)");

  DoDemangleTest("__opQ310WiimoteEmu10Shinkansen10DataFormat__Q26Common61BitCastPtrType<"
                 "Q310WiimoteEmu10Shinkansen10DataFormat,A21_Uc>Fv",
                 "Common::BitCastPtrType<WiimoteEmu::Shinkansen::DataFormat, unsigned "
                 "char[21]>::operator WiimoteEmu::Shinkansen::DataFormat()");

  DoDemangleTest(
      "__ct__Q26Common61BitCastPtrType<Q310WiimoteEmu10Shinkansen10DataFormat,A21_Uc>FPA21_Uc",
      "Common::BitCastPtrType<WiimoteEmu::Shinkansen::DataFormat, unsigned "
      "char[21]>::BitCastPtrType(unsigned char(*)[21])");

  DoDemangleTest("__pp__Q36Common10BitSet<Uc>8IteratorFi",
                 "Common::BitSet<unsigned char>::Iterator::operator++(int)");

  DoDemangleTest("__ne__Q36Common10BitSet<Uc>8IteratorFQ36Common10BitSet<Uc>8Iterator",
                 "Common::BitSet<unsigned char>::Iterator::operator!=(Common::BitSet<unsigned "
                 "char>::Iterator)");

  DoDemangleTest("__lt__Q26Common10BitSet<Uc>FQ26Common10BitSet<Uc>",
                 "Common::BitSet<unsigned char>::operator<(Common::BitSet<unsigned char>)");

  DoDemangleTest("__opb__Q26Common10BitSet<Uc>Fv",
                 "Common::BitSet<unsigned char>::operator bool()");

  DoDemangleTest("AddChildCodeSpace__Q26Common28CodeBlock<Q23Gen8XEmitter,1>FPQ26Common28CodeBlock<"
                 "Q23Gen8XEmitter,1>Ux",
                 "Common::CodeBlock<Gen::XEmitter, "
                 "1>::AddChildCodeSpace(Common::CodeBlock<Gen::XEmitter, 1>*, unsigned long long)");

  DoDemangleTest(
      "__ct__Q26Common505Lazy<Q23std490unordered_map<Q23std59basic_string<c,Q23std14char_traits<c>,"
      "Q23std12allocator<c>>,Q23std59basic_string<c,Q23std14char_traits<c>,Q23std12allocator<c>>,"
      "Q23std73hash<Q23std59basic_string<c,Q23std14char_traits<c>,Q23std12allocator<c>>>,"
      "Q23std77equal_to<Q23std59basic_string<c,Q23std14char_traits<c>,Q23std12allocator<c>>>,"
      "Q23std162allocator<Q23std142pair<CQ23std59basic_string<c,Q23std14char_traits<c>,"
      "Q23std12allocator<c>>,Q23std59basic_string<c,Q23std14char_traits<c>,Q23std12allocator<c>>>>>"
      ">Fv",
      "Common::Lazy<std::unordered_map<std::basic_string<char, std::char_traits<char>, "
      "std::allocator<char>>, std::basic_string<char, std::char_traits<char>, "
      "std::allocator<char>>, std::hash<std::basic_string<char, std::char_traits<char>, "
      "std::allocator<char>>>, std::equal_to<std::basic_string<char, std::char_traits<char>, "
      "std::allocator<char>>>, std::allocator<std::pair<const std::basic_string<char, "
      "std::char_traits<char>, std::allocator<char>>, std::basic_string<char, "
      "std::char_traits<char>, std::allocator<char>>>>>>::Lazy()");

  DoDemangleTest(
      "Append__Q26Common59LinearDiskCache<37ShaderUid<24geometry_shader_uid_data>,Uc>"
      "FRC37ShaderUid<24geometry_shader_uid_data>PCUcUl",
      "Common::LinearDiskCache<ShaderUid<geometry_shader_uid_data>, unsigned char>::Append(const "
      "ShaderUid<geometry_shader_uid_data>&, const unsigned char*, unsigned long)");

  DoDemangleTest("SetIsWii__Q24Core6SystemFb", "Core::System::SetIsWii(bool)");

  DoDemangleTest(
      "SetDOLFromFile__Q26DiscIO22DirectoryBlobPartitionFRQ23std59basic_string<c,Q23std14char_"
      "traits<c>,Q23std12allocator<c>>UxPQ23std32vector<Uc,Q23std13allocator<Uc>>",
      "DiscIO::DirectoryBlobPartition::SetDOLFromFile(std::basic_string<char, "
      "std::char_traits<char>, std::allocator<char>>&, unsigned long long, std::vector<unsigned "
      "char, std::allocator<unsigned char>>*)");

  DoDemangleTest(
      "SetDOL__Q26DiscIO22DirectoryBlobPartitionFQ26DiscIO14FSTBuilderNodeUxPQ23std32vector<Uc,"
      "Q23std13allocator<Uc>>",
      "DiscIO::DirectoryBlobPartition::SetDOL(DiscIO::FSTBuilderNode, unsigned long long, "
      "std::vector<unsigned char, std::allocator<unsigned char>>*)");

  DoDemangleTest("ReadFromGroups__Q26DiscIO19WIARVZFileReader<1>FPUxPUxPPUcUxUlUxUxUlUlUl",
                 "DiscIO::WIARVZFileReader<1>::ReadFromGroups(unsigned long long*, unsigned long "
                 "long*, unsigned char**, unsigned long long, unsigned long, unsigned long long, "
                 "unsigned long long, unsigned long, unsigned long, unsigned long)");

  DoDemangleTest("GetCompanyFromID__6DiscIOFRCQ23std59basic_string<c,Q23std14char_traits<c>,"
                 "Q23std12allocator<c>>",
                 "DiscIO::GetCompanyFromID(const std::basic_string<char, std::char_traits<char>, "
                 "std::allocator<char>>&)");

  DoDemangleTest("DiscordJoinRequest__Q27Discord7HandlerFPcRQ23std59basic_string<c,Q23std14char_"
                 "traits<c>,Q23std12allocator<c>>Pc",
                 "Discord::Handler::DiscordJoinRequest(char*, std::basic_string<char, "
                 "std::char_traits<char>, std::allocator<char>>&, char*)");

  DoDemangleTest("AddVoice__Q33DSP3HLE18ZeldaAudioRendererFUs",
                 "DSP::HLE::ZeldaAudioRenderer::AddVoice(unsigned short)");

  DoDemangleTest("SetConstPatterns__Q33DSP3HLE18ZeldaAudioRendererFPQ23std12array<s,256>",
                 "DSP::HLE::ZeldaAudioRenderer::SetConstPatterns(std::array<short, 256>*)");

  DoDemangleTest("ReJitConditional__Q33DSP3x6410DSPEmitterFUsMQ33DSP3x6410DSPEmitterFPCvPvUs_v",
                 "DSP::x64::DSPEmitter::ReJitConditional(unsigned short, void "
                 "(DSP::x64::DSPEmitter::*)(unsigned short))");

  DoDemangleTest("decode_mb__9ERContextFPviiiPA4_A2_Piiiii",
                 "ERContext::decode_mb(void*, int, int, int, int*(*)[4][2], int, int, int, int)");

  DoDemangleTest("Write__Q24File12DirectIOFileFQ23std11span<Uc,-1>",
                 "File::DirectIOFile::Write(std::span<unsigned char, -1>)");

  DoDemangleTest("SetBoth__Q27PowerPC12PairedSingleFff",
                 "PowerPC::PairedSingle::SetBoth(float, float)");

  DoDemangleTest("GetPaletteConversionPipeline__Q211VideoCommon11ShaderCacheF10TLUTFormat",
                 "VideoCommon::ShaderCache::GetPaletteConversionPipeline(TLUTFormat)");

  DoDemangleTest(
      "GetGXPipelineConfig__"
      "Q211VideoCommon11ShaderCacheFP18NativeVertexFormatP14AbstractShaderP14AbstractShaderP14Abstr"
      "actShaderR18RasterizationStateR10DepthStateR13BlendingState21AbstractPipelineUsage",
      "VideoCommon::ShaderCache::GetGXPipelineConfig(NativeVertexFormat*, AbstractShader*, "
      "AbstractShader*, AbstractShader*, RasterizationState&, DepthState&, BlendingState&, "
      "AbstractPipelineUsage)");

  DoDemangleTest("LoadCaches__Q211VideoCommon11ShaderCacheFv",
                 "VideoCommon::ShaderCache::LoadCaches()");

  DoDemangleTest(
      "LoadPipelineCache<Q211VideoCommon17GXUberPipelineUid,"
      "Q211VideoCommon27SerializedGXUberPipelineUid,Q23std355map<"
      "Q211VideoCommon17GXUberPipelineUid,Q23std89pair<Q23std73unique_ptr<16AbstractPipeline,"
      "Q23std34default_delete<16AbstractPipeline>>,b>,Q23std40less<"
      "Q211VideoCommon17GXUberPipelineUid>,Q23std159allocator<Q23std139pair<"
      "CQ211VideoCommon17GXUberPipelineUid,Q23std89pair<Q23std73unique_ptr<16AbstractPipeline,"
      "Q23std34default_delete<16AbstractPipeline>>,b>>>>>__"
      "Q211VideoCommon11ShaderCacheFRQ23std355map<Q211VideoCommon17GXUberPipelineUid,Q23std89pair<"
      "Q23std73unique_ptr<16AbstractPipeline,Q23std34default_delete<16AbstractPipeline>>,b>,"
      "Q23std40less<Q211VideoCommon17GXUberPipelineUid>,Q23std159allocator<Q23std139pair<"
      "CQ211VideoCommon17GXUberPipelineUid,Q23std89pair<Q23std73unique_ptr<16AbstractPipeline,"
      "Q23std34default_delete<16AbstractPipeline>>,b>>>>RQ26Common64LinearDiskCache<"
      "Q211VideoCommon27SerializedGXUberPipelineUid,Uc>7APITypePCcb_v",
      "void VideoCommon::ShaderCache::LoadPipelineCache<VideoCommon::GXUberPipelineUid, "
      "VideoCommon::SerializedGXUberPipelineUid, std::map<VideoCommon::GXUberPipelineUid, "
      "std::pair<std::unique_ptr<AbstractPipeline, std::default_delete<AbstractPipeline>>, bool>, "
      "std::less<VideoCommon::GXUberPipelineUid>, std::allocator<std::pair<const "
      "VideoCommon::GXUberPipelineUid, std::pair<std::unique_ptr<AbstractPipeline, "
      "std::default_delete<AbstractPipeline>>, bool>>>>>(std::map<VideoCommon::GXUberPipelineUid, "
      "std::pair<std::unique_ptr<AbstractPipeline, std::default_delete<AbstractPipeline>>, bool>, "
      "std::less<VideoCommon::GXUberPipelineUid>, std::allocator<std::pair<const "
      "VideoCommon::GXUberPipelineUid, std::pair<std::unique_ptr<AbstractPipeline, "
      "std::default_delete<AbstractPipeline>>, bool>>>>&, "
      "Common::LinearDiskCache<VideoCommon::SerializedGXUberPipelineUid, unsigned char>&, APIType, "
      "const char*, bool)");

  DoDemangleTest(
      "BuildDesiredExtensionState__Q210WiimoteEmu7TaTaConFPQ210WiimoteEmu21DesiredExtensionState",
      "WiimoteEmu::TaTaCon::BuildDesiredExtensionState(WiimoteEmu::DesiredExtensionState*)");

  DoDemangleTest("__ct__Q210WiimoteEmu7WiimoteFUl", "WiimoteEmu::Wiimote::Wiimote(unsigned long)");

  DoDemangleTest("SetWiimoteDeviceIndex__Q210WiimoteEmu7WiimoteFUc",
                 "WiimoteEmu::Wiimote::SetWiimoteDeviceIndex(unsigned char)");

  DoDemangleTest("GetAngularVelocity__Q210WiimoteEmu7WiimoteFQ26Common8TVec3<f>",
                 "WiimoteEmu::Wiimote::GetAngularVelocity(Common::TVec3<float>)");

  DoDemangleTest("BareFn__FPFPCcPv_v_v", "void BareFn(void (*)(const char*, void*))");
  DoDemangleTest("BareFn__FPFPCcPv_v_PFPCvPv_v",
                 "void (* BareFn(void (*)(const char*, void*)))(const void*, void*)");
  DoDemangleTest(
      "SomeFn__FRCPFPFPCvPv_v_RCPFPCvPv_v",
      "SomeFn(void (*const& (*const&)(void (*)(const void*, void*)))(const void*, void*))");
  DoDemangleTest(
      "SomeFn__Q29Namespace5ClassCFRCMQ29Namespace5ClassFPCvPCvMQ29Namespace5ClassFPCvPCvPCvPv_"
      "v_RCMQ29Namespace5ClassFPCvPCvPCvPv_v",
      "Namespace::Class::SomeFn(void (Namespace::Class::*const & (Namespace::Class::*const "
      "&)(void (Namespace::Class::*)(const void*, void*) const) const)(const void*, void*) "
      "const) const");
  DoDemangleTest("Matrix__FfPA2_A3_f", "Matrix(float, float(*)[2][3])");
  DoDemangleTest("test__FRCPCPCi", "test(const int* const* const&)");

  DoDemangleTest("@GUARD@GetCompanyFromID__6DiscIOFRCQ23std59basic_string<c,Q23std14char_traits<c>,"
                 "Q23std12allocator<c>>@EMPTY_STRING",
                 "DiscIO::GetCompanyFromID(const std::basic_string<char, std::char_traits<char>, "
                 "std::allocator<char>>&)::EMPTY_STRING guard");

  DoDemangleTest("@LOCAL@GetCompanyFromID__6DiscIOFRCQ23std59basic_string<c,Q23std14char_traits<c>,"
                 "Q23std12allocator<c>>@EMPTY_STRING",
                 "DiscIO::GetCompanyFromID(const std::basic_string<char, std::char_traits<char>, "
                 "std::allocator<char>>&)::EMPTY_STRING");

  DoDemangleTest("@LOCAL@SetSIMDMode__Q26Common3FPUFQ36Common3FPU9RoundModeb@EXCEPTION_MASK",
                 "Common::FPU::SetSIMDMode(Common::FPU::RoundMode, bool)::EXCEPTION_MASK");

  DoDemangleTest("@LOCAL@GetInstance__18AchievementManagerFv@s_instance",
                 "AchievementManager::GetInstance()::s_instance");

  DoDemangleTest("s_instance$localstatic3$GetInstance__18AchievementManagerFv",
                 "AchievementManager::GetInstance()::s_instance");

  DoDemangleTest("init$localstatic4$GetInstance__18AchievementManagerFv",
                 "AchievementManager::GetInstance()::localstatic4 guard");
}

TEST(CWDemangler, TestDemangleOptions)
{
  DoDemangleOptionsTest(true, false, "__dt__Q210WiimoteEmu7WiimoteFv",
                        "WiimoteEmu::Wiimote::~Wiimote()");
  DoDemangleOptionsTest(false, false, "__dt__Q210WiimoteEmu7WiimoteFv",
                        "WiimoteEmu::Wiimote::~Wiimote(void)");
  DoDemangleOptionsTest(true, true, "example__10HahaOne<1>CFv",
                        "HahaOne<__int128>::example() const");
  DoDemangleOptionsTest(true, true, "fn<3,PV2>__FPC2",
                        "fn<3, volatile __vec2x32float__*>(const __vec2x32float__*)");
}

#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <tuple>

#include "Common/CWDemangler.h"

void DoDemangleTemplateArgsTest(std::string mangled, std::string name, std::string templateArgs)
{
  DemangleOptions options = DemangleOptions();

  auto result = CWDemangler::demangle_template_args(mangled, options);
  std::optional<std::tuple<std::string, std::string>> expected = {{name, templateArgs}};

  EXPECT_EQ(result, expected);
}

void DoDemangleNameTest(std::string mangled, std::string name, std::string fullName)
{
  DemangleOptions options = DemangleOptions();

  auto result = CWDemangler::demangle_name(mangled, options);
  std::optional<std::tuple<std::string, std::string, std::string>> expected = {
      {name, fullName, ""}};

  EXPECT_EQ(result, expected);
}

void DoDemangleQualifiedNameTest(std::string mangled, std::string baseName, std::string fullName)
{
  DemangleOptions options = DemangleOptions();

  auto result = CWDemangler::demangle_qualified_name(mangled, options);
  std::optional<std::tuple<std::string, std::string, std::string>> expected = {
      {baseName, fullName, ""}};

  EXPECT_EQ(result, expected);
}

void DoDemangleArgTest(std::string mangled, std::string typePre, std::string typePost,
                       std::string remainder)
{
  DemangleOptions options = DemangleOptions();

  auto result = CWDemangler::demangle_arg(mangled, options);
  std::optional<std::tuple<std::string, std::string, std::string>> expected = {
      {typePre, typePost, remainder}};

  EXPECT_EQ(result, expected);
}

void DoDemangleFunctionArgsTest(std::string mangled, std::string args, std::string remainder)
{
  DemangleOptions options = DemangleOptions();

  auto result = CWDemangler::demangle_function_args(mangled, options);
  std::optional<std::tuple<std::string, std::string>> expected = {{args, remainder}};

  EXPECT_EQ(result, expected);
}

void DoDemangleTest(std::string mangled, std::string demangled)
{
  DemangleOptions options = DemangleOptions();

  auto result = CWDemangler::demangle(mangled, options);
  std::optional<std::string> expected = {demangled};
  if (demangled == "")
    expected = std::nullopt;

  EXPECT_EQ(result, expected);
}

void DoDemangleOptionsTest(bool omitEmptyParams, bool mwExtensions, std::string mangled,
                           std::string demangled)
{
  DemangleOptions options = DemangleOptions(omitEmptyParams, mwExtensions);

  auto result = CWDemangler::demangle(mangled, options);
  std::optional<std::string> expected = {demangled};

  EXPECT_EQ(result, expected);
}

TEST(CWDemangler, TestDemangleTemplateArgs)
{
  DoDemangleTemplateArgsTest("single_ptr<10CModelData>", "single_ptr", "<CModelData>");
  DoDemangleTemplateArgsTest("basic_string<w,Q24rstl14char_traits<w>,Q24rstl17rmemory_allocator>",
                             "basic_string",
                             "<wchar_t, rstl::char_traits<wchar_t>, rstl::rmemory_allocator>");
}

TEST(CWDemangler, TestDemangleName)
{
  DoDemangleNameTest("24single_ptr<10CModelData>", "single_ptr", "single_ptr<CModelData>");
  DoDemangleNameTest("66basic_string<w,Q24rstl14char_traits<w>,Q24rstl17rmemory_allocator>",
                     "basic_string",
                     "basic_string<wchar_t, rstl::char_traits<wchar_t>, rstl::rmemory_allocator>");
}

TEST(CWDemangler, TestDemangleQualifiedName)
{
  DoDemangleQualifiedNameTest("6CActor", "CActor", "CActor");
  DoDemangleQualifiedNameTest("Q29CVector3f4EDim", "EDim", "CVector3f::EDim");
  DoDemangleQualifiedNameTest(
      "Q24rstl66basic_string<w,Q24rstl14char_traits<w>,Q24rstl17rmemory_allocator>", "basic_string",
      "rstl::basic_string<wchar_t, rstl::char_traits<wchar_t>, rstl::rmemory_allocator>");
}

TEST(CWDemangler, TestDemangleArg)
{
  DoDemangleArgTest("v", "void", "", "");
  DoDemangleArgTest("b", "bool", "", "");
  DoDemangleArgTest("RC9CVector3fUc", "const CVector3f&", "", "Uc");
  DoDemangleArgTest("Q24rstl14char_traits<w>,", "rstl::char_traits<wchar_t>", "", ",");
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
  DoDemangleTest("__dt__6CActorFv", "CActor::~CActor()");
  DoDemangleTest("GetSfxHandle__6CActorCFv", "CActor::GetSfxHandle() const");
  DoDemangleTest(
      "mNull__Q24rstl66basic_string<w,Q24rstl14char_traits<w>,Q24rstl17rmemory_allocator>",
      "rstl::basic_string<wchar_t, rstl::char_traits<wchar_t>, "
      "rstl::rmemory_allocator>::mNull");
  DoDemangleTest(
      "__ct__Q34rstl495red_black_tree<Ux,Q24rstl194pair<Ux,Q24rstl175auto_ptr<Q24rstl155map<s,"
      "Q24rstl96auto_ptr<Q24rstl77list<Q24rstl35auto_ptr<23CGuiFrameMessageMapNode>,"
      "Q24rstl17rmemory_allocator>>,Q24rstl7less<s>,Q24rstl17rmemory_allocator>>>,0,"
      "Q24rstl215select1st<Q24rstl194pair<Ux,Q24rstl175auto_ptr<Q24rstl155map<s,Q24rstl96auto_"
      "ptr<Q24rstl77list<Q24rstl35auto_ptr<23CGuiFrameMessageMapNode>,Q24rstl17rmemory_"
      "allocator>>,Q24rstl7less<s>,Q24rstl17rmemory_allocator>>>>,Q24rstl8less<Ux>,"
      "Q24rstl17rmemory_allocator>8iteratorFPQ34rstl495red_black_tree<Ux,Q24rstl194pair<Ux,"
      "Q24rstl175auto_ptr<Q24rstl155map<s,Q24rstl96auto_ptr<Q24rstl77list<Q24rstl35auto_ptr<"
      "23CGuiFrameMessageMapNode>,Q24rstl17rmemory_allocator>>,Q24rstl7less<s>,"
      "Q24rstl17rmemory_allocator>>>,0,Q24rstl215select1st<Q24rstl194pair<Ux,Q24rstl175auto_"
      "ptr<Q24rstl155map<s,Q24rstl96auto_ptr<Q24rstl77list<Q24rstl35auto_ptr<"
      "23CGuiFrameMessageMapNode>,Q24rstl17rmemory_allocator>>,Q24rstl7less<s>,"
      "Q24rstl17rmemory_allocator>>>>,Q24rstl8less<Ux>,Q24rstl17rmemory_allocator>"
      "4nodePCQ34rstl495red_black_tree<Ux,Q24rstl194pair<Ux,Q24rstl175auto_ptr<Q24rstl155map<s,"
      "Q24rstl96auto_ptr<Q24rstl77list<Q24rstl35auto_ptr<23CGuiFrameMessageMapNode>,"
      "Q24rstl17rmemory_allocator>>,Q24rstl7less<s>,Q24rstl17rmemory_allocator>>>,0,"
      "Q24rstl215select1st<Q24rstl194pair<Ux,Q24rstl175auto_ptr<Q24rstl155map<s,Q24rstl96auto_"
      "ptr<Q24rstl77list<Q24rstl35auto_ptr<23CGuiFrameMessageMapNode>,Q24rstl17rmemory_"
      "allocator>>,Q24rstl7less<s>,Q24rstl17rmemory_allocator>>>>,Q24rstl8less<Ux>,"
      "Q24rstl17rmemory_allocator>6header",
      "rstl::red_black_tree<unsigned long long, rstl::pair<unsigned long long, "
      "rstl::auto_ptr<rstl::map<short, "
      "rstl::auto_ptr<rstl::list<rstl::auto_ptr<CGuiFrameMessageMapNode>, "
      "rstl::rmemory_allocator>>, rstl::less<short>, rstl::rmemory_allocator>>>, 0, "
      "rstl::select1st<rstl::pair<unsigned long long, rstl::auto_ptr<rstl::map<short, "
      "rstl::auto_ptr<rstl::list<rstl::auto_ptr<CGuiFrameMessageMapNode>, "
      "rstl::rmemory_allocator>>, rstl::less<short>, rstl::rmemory_allocator>>>>, "
      "rstl::less<unsigned long long>, "
      "rstl::rmemory_allocator>::iterator::iterator(rstl::red_black_tree<unsigned long long, "
      "rstl::pair<unsigned long long, rstl::auto_ptr<rstl::map<short, "
      "rstl::auto_ptr<rstl::list<rstl::auto_ptr<CGuiFrameMessageMapNode>, "
      "rstl::rmemory_allocator>>, rstl::less<short>, rstl::rmemory_allocator>>>, 0, "
      "rstl::select1st<rstl::pair<unsigned long long, rstl::auto_ptr<rstl::map<short, "
      "rstl::auto_ptr<rstl::list<rstl::auto_ptr<CGuiFrameMessageMapNode>, "
      "rstl::rmemory_allocator>>, rstl::less<short>, rstl::rmemory_allocator>>>>, "
      "rstl::less<unsigned long long>, rstl::rmemory_allocator>::node*, const "
      "rstl::red_black_tree<unsigned long long, rstl::pair<unsigned long long, "
      "rstl::auto_ptr<rstl::map<short, "
      "rstl::auto_ptr<rstl::list<rstl::auto_ptr<CGuiFrameMessageMapNode>, "
      "rstl::rmemory_allocator>>, rstl::less<short>, rstl::rmemory_allocator>>>, 0, "
      "rstl::select1st<rstl::pair<unsigned long long, rstl::auto_ptr<rstl::map<short, "
      "rstl::auto_ptr<rstl::list<rstl::auto_ptr<CGuiFrameMessageMapNode>, "
      "rstl::rmemory_allocator>>, rstl::less<short>, rstl::rmemory_allocator>>>>, "
      "rstl::less<unsigned long long>, rstl::rmemory_allocator>::header*)");
  DoDemangleTest(
      "for_each<PP12MultiEmitter,Q23std51binder2nd<Q23std30mem_fun1_t<v,12MultiEmitter,l>,"
      "l>>__3stdFPP12MultiEmitterPP12MultiEmitterQ23std51binder2nd<Q23std30mem_fun1_t<v,"
      "12MultiEmitter,l>,l>_Q23std51binder2nd<Q23std30mem_fun1_t<v,12MultiEmitter,l>,l>",
      "std::binder2nd<std::mem_fun1_t<void, MultiEmitter, long>, long> "
      "std::for_each<MultiEmitter**, std::binder2nd<std::mem_fun1_t<void, MultiEmitter, "
      "long>, long>>(MultiEmitter**, MultiEmitter**, std::binder2nd<std::mem_fun1_t<void, "
      "MultiEmitter, long>, long>)");
  DoDemangleTest(
      "__ct__Q43std3tr16detail383function_imp<PFPCcPCc_v,Q43std3tr16detail334bound_func<v,"
      "Q43std3tr16detail59mem_fn_2<v,Q53scn4step7gimmick9shipevent9ShipEvent,PCc,PCc>,"
      "Q33std3tr1228tuple<PQ53scn4step7gimmick9shipevent9ShipEvent,"
      "Q53std3tr112placeholders6detail5ph<1>,Q53std3tr112placeholders6detail5ph<2>,"
      "Q33std3tr13nat,Q33std3tr13nat,Q33std3tr13nat,Q33std3tr13nat,Q33std3tr13nat,"
      "Q33std3tr13nat,Q33std3tr13nat>>,0,1>FRCQ43std3tr16detail383function_imp<PFPCcPCc_v,"
      "Q43std3tr16detail334bound_func<v,Q43std3tr16detail59mem_fn_2<v,"
      "Q53scn4step7gimmick9shipevent9ShipEvent,PCc,PCc>,Q33std3tr1228tuple<"
      "PQ53scn4step7gimmick9shipevent9ShipEvent,Q53std3tr112placeholders6detail5ph<1>,"
      "Q53std3tr112placeholders6detail5ph<2>,Q33std3tr13nat,Q33std3tr13nat,Q33std3tr13nat,"
      "Q33std3tr13nat,Q33std3tr13nat,Q33std3tr13nat,Q33std3tr13nat>>,0,1>",
      "std::tr1::detail::function_imp<void (*)(const char*, const char*), "
      "std::tr1::detail::bound_func<void, std::tr1::detail::mem_fn_2<void, "
      "scn::step::gimmick::shipevent::ShipEvent, const char*, const char*>, "
      "std::tr1::tuple<scn::step::gimmick::shipevent::ShipEvent*, "
      "std::tr1::placeholders::detail::ph<1>, std::tr1::placeholders::detail::ph<2>, "
      "std::tr1::nat, std::tr1::nat, std::tr1::nat, std::tr1::nat, std::tr1::nat, "
      "std::tr1::nat, std::tr1::nat>>, 0, 1>::function_imp(const "
      "std::tr1::detail::function_imp<void (*)(const char*, const char*), "
      "std::tr1::detail::bound_func<void, std::tr1::detail::mem_fn_2<void, "
      "scn::step::gimmick::shipevent::ShipEvent, const char*, const char*>, "
      "std::tr1::tuple<scn::step::gimmick::shipevent::ShipEvent*, "
      "std::tr1::placeholders::detail::ph<1>, std::tr1::placeholders::detail::ph<2>, "
      "std::tr1::nat, std::tr1::nat, std::tr1::nat, std::tr1::nat, std::tr1::nat, "
      "std::tr1::nat, std::tr1::nat>>, 0, 1>&)");
  DoDemangleTest(
      "createJointController<11IKJointCtrl>__"
      "2MRFP11IKJointCtrlPC9LiveActorUsM11IKJointCtrlFPCvPvPQ29JGeometry64TPosition3<"
      "Q29JGeometry38TMatrix34<Q29JGeometry13SMatrix34C<f>>>RC19JointControllerInfo_"
      "bM11IKJointCtrlFPCvPvPQ29JGeometry64TPosition3<Q29JGeometry38TMatrix34<"
      "Q29JGeometry13SMatrix34C<f>>>RC19JointControllerInfo_b_P15JointController",
      "JointController* MR::createJointController<IKJointCtrl>(IKJointCtrl*, const "
      "LiveActor*, unsigned short, bool "
      "(IKJointCtrl::*)(JGeometry::TPosition3<JGeometry::TMatrix34<JGeometry::SMatrix34C<"
      "float>>>*, const JointControllerInfo&), bool "
      "(IKJointCtrl::*)(JGeometry::TPosition3<JGeometry::TMatrix34<JGeometry::SMatrix34C<"
      "float>>>*, const JointControllerInfo&))");
  DoDemangleTest("execCommand__12JASSeqParserFP8JASTrackM12JASSeqParserFPCvPvP8JASTrackPUl_lUlPUl",
                 "JASSeqParser::execCommand(JASTrack*, long (JASSeqParser::*)(JASTrack*, unsigned "
                 "long*), unsigned long, unsigned long*)");
  DoDemangleTest("AddWidgetFnMap__"
                 "10CGuiWidgetFiM10CGuiWidgetFPCvPvP15CGuiFunctionDefP18CGuiControllerInfo_i",
                 "CGuiWidget::AddWidgetFnMap(int, int (CGuiWidget::*)(CGuiFunctionDef*, "
                 "CGuiControllerInfo*))");
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
  DoDemangleTest("__pl__FRC9CRelAngleRC9CRelAngle",
                 "operator+(const CRelAngle&, const CRelAngle&)");
  DoDemangleTest("destroy<PUi>__4rstlFPUiPUi",
                 "rstl::destroy<unsigned int*>(unsigned int*, unsigned int*)");
  DoDemangleTest("__opb__33TFunctor2<CP15CGuiSliderGroup,Cf>CFv",
                 "TFunctor2<CGuiSliderGroup* const, const float>::operator bool() const");
  DoDemangleTest("__opRC25TToken<15CCharLayoutInfo>__31TLockedToken<15CCharLayoutInfo>CFv",
                 "TLockedToken<CCharLayoutInfo>::operator const TToken<CCharLayoutInfo>&() const");
  DoDemangleTest(
      "uninitialized_copy<Q24rstl198pointer_iterator<"
      "Q224CSpawnSystemKeyframeData24CSpawnSystemKeyframeInfo,Q24rstl89vector<"
      "Q224CSpawnSystemKeyframeData24CSpawnSystemKeyframeInfo,Q24rstl17rmemory_allocator>,"
      "Q24rstl17rmemory_allocator>,PQ224CSpawnSystemKeyframeData24CSpawnSystemKeyframeInfo>__"
      "4rstlFQ24rstl198pointer_iterator<Q224CSpawnSystemKeyframeData24CSpawnSystemKeyframeInfo,"
      "Q24rstl89vector<Q224CSpawnSystemKeyframeData24CSpawnSystemKeyframeInfo,Q24rstl17rmemory_"
      "allocator>,Q24rstl17rmemory_allocator>Q24rstl198pointer_iterator<"
      "Q224CSpawnSystemKeyframeData24CSpawnSystemKeyframeInfo,Q24rstl89vector<"
      "Q224CSpawnSystemKeyframeData24CSpawnSystemKeyframeInfo,Q24rstl17rmemory_allocator>,"
      "Q24rstl17rmemory_allocator>PQ224CSpawnSystemKeyframeData24CSpawnSystemKeyframeInfo",
      "rstl::uninitialized_copy<rstl::pointer_iterator<CSpawnSystemKeyframeData::"
      "CSpawnSystemKeyframeInfo, "
      "rstl::vector<CSpawnSystemKeyframeData::CSpawnSystemKeyframeInfo, "
      "rstl::rmemory_allocator>, rstl::rmemory_allocator>, "
      "CSpawnSystemKeyframeData::CSpawnSystemKeyframeInfo*>(rstl::pointer_iterator<"
      "CSpawnSystemKeyframeData::CSpawnSystemKeyframeInfo, "
      "rstl::vector<CSpawnSystemKeyframeData::CSpawnSystemKeyframeInfo, "
      "rstl::rmemory_allocator>, rstl::rmemory_allocator>, "
      "rstl::pointer_iterator<CSpawnSystemKeyframeData::CSpawnSystemKeyframeInfo, "
      "rstl::vector<CSpawnSystemKeyframeData::CSpawnSystemKeyframeInfo, "
      "rstl::rmemory_allocator>, rstl::rmemory_allocator>, "
      "CSpawnSystemKeyframeData::CSpawnSystemKeyframeInfo*)");
  DoDemangleTest(
      "__rf__Q34rstl120list<Q24rstl78pair<i,PFRC10SObjectTagR12CInputStreamRC15CVParamTransfer_"
      "C16CFactoryFnReturn>,Q24rstl17rmemory_allocator>14const_iteratorCFv",
      "rstl::list<rstl::pair<int, const CFactoryFnReturn (*)(const SObjectTag&, CInputStream&, "
      "const CVParamTransfer&)>, rstl::rmemory_allocator>::const_iterator::operator->() const");
  DoDemangleTest(
      "ApplyRipples__FRC14CRippleManagerRA43_A43_Q220CFluidPlaneCPURender13SHFieldSampleRA22_"
      "A22_UcRA256_CfRQ220CFluidPlaneCPURender10SPatchInfo",
      "ApplyRipples(const CRippleManager&, CFluidPlaneCPURender::SHFieldSample(&)[43][43], "
      "unsigned char(&)[22][22], const float(&)[256], CFluidPlaneCPURender::SPatchInfo&)");
  DoDemangleTest("CalculateFluidTextureOffset__14CFluidUVMotionCFfPA2_f",
                 "CFluidUVMotion::CalculateFluidTextureOffset(float, float(*)[2]) const");
  DoDemangleTest(
      "RenderNormals__FRA43_A43_CQ220CFluidPlaneCPURender13SHFieldSampleRA22_A22_"
      "CUcRCQ220CFluidPlaneCPURender10SPatchInfo",
      "RenderNormals(const CFluidPlaneCPURender::SHFieldSample(&)[43][43], const unsigned "
      "char(&)[22][22], const CFluidPlaneCPURender::SPatchInfo&)");
  DoDemangleTest("Matrix__FfPA2_A3_f", "Matrix(float, float(*)[2][3])");
  DoDemangleTest("__ct<12CStringTable>__31CObjOwnerDerivedFromIObjUntypedFRCQ24rstl24auto_ptr<"
                 "12CStringTable>",
                 "CObjOwnerDerivedFromIObjUntyped::CObjOwnerDerivedFromIObjUntyped<CStringTable>("
                 "const rstl::auto_ptr<CStringTable>&)");
  DoDemangleTest("__vt__40TObjOwnerDerivedFromIObj<12CStringTable>",
                 "TObjOwnerDerivedFromIObj<CStringTable>::__vtable");
  DoDemangleTest("__RTTI__40TObjOwnerDerivedFromIObj<12CStringTable>",
                 "TObjOwnerDerivedFromIObj<CStringTable>::__RTTI");
  DoDemangleTest("__init__mNull__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_"
                 "allocator>",
                 "rstl::basic_string<char, rstl::char_traits<char>, "
                 "rstl::rmemory_allocator>::__init__mNull");
  DoDemangleTest("__dt__26__partial_array_destructorFv",
                 "__partial_array_destructor::~__partial_array_destructor()");
  DoDemangleTest(
      "__distance<Q34rstl195red_black_tree<13TGameScriptId,Q24rstl32pair<13TGameScriptId,"
      "9TUniqueId>,1,Q24rstl52select1st<Q24rstl32pair<13TGameScriptId,9TUniqueId>>,"
      "Q24rstl21less<13TGameScriptId>,Q24rstl17rmemory_allocator>14const_iterator>__"
      "4rstlFQ34rstl195red_black_tree<13TGameScriptId,Q24rstl32pair<13TGameScriptId,9TUniqueId>"
      ",1,Q24rstl52select1st<Q24rstl32pair<13TGameScriptId,9TUniqueId>>,Q24rstl21less<"
      "13TGameScriptId>,Q24rstl17rmemory_allocator>14const_iteratorQ34rstl195red_black_tree<"
      "13TGameScriptId,Q24rstl32pair<13TGameScriptId,9TUniqueId>,1,Q24rstl52select1st<"
      "Q24rstl32pair<13TGameScriptId,9TUniqueId>>,Q24rstl21less<13TGameScriptId>,"
      "Q24rstl17rmemory_allocator>14const_iteratorQ24rstl20forward_iterator_tag",
      "rstl::__distance<rstl::red_black_tree<TGameScriptId, rstl::pair<TGameScriptId, "
      "TUniqueId>, 1, rstl::select1st<rstl::pair<TGameScriptId, TUniqueId>>, "
      "rstl::less<TGameScriptId>, "
      "rstl::rmemory_allocator>::const_iterator>(rstl::red_black_tree<TGameScriptId, "
      "rstl::pair<TGameScriptId, TUniqueId>, 1, rstl::select1st<rstl::pair<TGameScriptId, "
      "TUniqueId>>, rstl::less<TGameScriptId>, rstl::rmemory_allocator>::const_iterator, "
      "rstl::red_black_tree<TGameScriptId, rstl::pair<TGameScriptId, TUniqueId>, 1, "
      "rstl::select1st<rstl::pair<TGameScriptId, TUniqueId>>, rstl::less<TGameScriptId>, "
      "rstl::rmemory_allocator>::const_iterator, rstl::forward_iterator_tag)");
  DoDemangleTest(
      "__ct__Q210Metrowerks683compressed_pair<RQ23std301allocator<Q33std276__tree_deleter<"
      "Q23std34pair<Ci,Q212petfurniture8Instance>,Q33std131__multimap_do_transform<i,"
      "Q212petfurniture8Instance,Q23std7less<i>,Q23std53allocator<Q23std34pair<Ci,"
      "Q212petfurniture8Instance>>,0>13value_compare,Q23std53allocator<Q23std34pair<Ci,"
      "Q212petfurniture8Instance>>>4node>,Q210Metrowerks337compressed_pair<"
      "Q210Metrowerks12number<Ul,1>,PQ33std276__tree_deleter<Q23std34pair<Ci,"
      "Q212petfurniture8Instance>,Q33std131__multimap_do_transform<i,Q212petfurniture8Instance,"
      "Q23std7less<i>,Q23std53allocator<Q23std34pair<Ci,Q212petfurniture8Instance>>,0>13value_"
      "compare,Q23std53allocator<Q23std34pair<Ci,Q212petfurniture8Instance>>>4node>>"
      "FRQ23std301allocator<Q33std276__tree_deleter<Q23std34pair<Ci,Q212petfurniture8Instance>,"
      "Q33std131__multimap_do_transform<i,Q212petfurniture8Instance,Q23std7less<i>,"
      "Q23std53allocator<Q23std34pair<Ci,Q212petfurniture8Instance>>,0>13value_compare,"
      "Q23std53allocator<Q23std34pair<Ci,Q212petfurniture8Instance>>>4node>"
      "Q210Metrowerks337compressed_pair<Q210Metrowerks12number<Ul,1>,PQ33std276__tree_deleter<"
      "Q23std34pair<Ci,Q212petfurniture8Instance>,Q33std131__multimap_do_transform<i,"
      "Q212petfurniture8Instance,Q23std7less<i>,Q23std53allocator<Q23std34pair<Ci,"
      "Q212petfurniture8Instance>>,0>13value_compare,Q23std53allocator<Q23std34pair<Ci,"
      "Q212petfurniture8Instance>>>4node>",
      "Metrowerks::compressed_pair<std::allocator<std::__tree_deleter<std::pair<const int, "
      "petfurniture::Instance>, std::__multimap_do_transform<int, petfurniture::Instance, "
      "std::less<int>, std::allocator<std::pair<const int, petfurniture::Instance>>, "
      "0>::value_compare, std::allocator<std::pair<const int, "
      "petfurniture::Instance>>>::node>&, "
      "Metrowerks::compressed_pair<Metrowerks::number<unsigned long, 1>, "
      "std::__tree_deleter<std::pair<const int, petfurniture::Instance>, "
      "std::__multimap_do_transform<int, petfurniture::Instance, std::less<int>, "
      "std::allocator<std::pair<const int, petfurniture::Instance>>, 0>::value_compare, "
      "std::allocator<std::pair<const int, "
      "petfurniture::Instance>>>::node*>>::compressed_pair(std::allocator<std::__tree_deleter<"
      "std::pair<const int, petfurniture::Instance>, std::__multimap_do_transform<int, "
      "petfurniture::Instance, std::less<int>, std::allocator<std::pair<const int, "
      "petfurniture::Instance>>, 0>::value_compare, std::allocator<std::pair<const int, "
      "petfurniture::Instance>>>::node>&, "
      "Metrowerks::compressed_pair<Metrowerks::number<unsigned long, 1>, "
      "std::__tree_deleter<std::pair<const int, petfurniture::Instance>, "
      "std::__multimap_do_transform<int, petfurniture::Instance, std::less<int>, "
      "std::allocator<std::pair<const int, petfurniture::Instance>>, 0>::value_compare, "
      "std::allocator<std::pair<const int, petfurniture::Instance>>>::node*>)");
  DoDemangleTest(
      "skBadString$localstatic3$GetNameByToken__31TTokenSet<18EScriptObjectState>"
      "CF18EScriptObjectState",
      "TTokenSet<EScriptObjectState>::GetNameByToken(EScriptObjectState) const::skBadString");
  DoDemangleTest("init$localstatic4$GetNameByToken__31TTokenSet<18EScriptObjectState>"
                 "CF18EScriptObjectState",
                 "TTokenSet<EScriptObjectState>::GetNameByToken(EScriptObjectState) "
                 "const::localstatic4 guard");
  DoDemangleTest("@LOCAL@GetAnmPlayPolicy__Q24nw4r3g3dFQ34nw4r3g3d9AnmPolicy@policyTable",
                 "nw4r::g3d::GetAnmPlayPolicy(nw4r::g3d::AnmPolicy)::policyTable");
  DoDemangleTest("@GUARD@GetAnmPlayPolicy__Q24nw4r3g3dFQ34nw4r3g3d9AnmPolicy@policyTable",
                 "nw4r::g3d::GetAnmPlayPolicy(nw4r::g3d::AnmPolicy)::policyTable guard");
  DoDemangleTest(
      "lower_bound<Q24rstl180const_pointer_iterator<Q24rstl33pair<Ui,22CAdditiveAnimationInfo>,"
      "Q24rstl77vector<Q24rstl33pair<Ui,22CAdditiveAnimationInfo>,Q24rstl17rmemory_allocator>,"
      "Q24rstl17rmemory_allocator>,Ui,Q24rstl79pair_sorter_finder<Q24rstl33pair<Ui,"
      "22CAdditiveAnimationInfo>,Q24rstl8less<Ui>>>__4rstlFQ24rstl180const_pointer_iterator<"
      "Q24rstl33pair<Ui,22CAdditiveAnimationInfo>,Q24rstl77vector<Q24rstl33pair<Ui,"
      "22CAdditiveAnimationInfo>,Q24rstl17rmemory_allocator>,Q24rstl17rmemory_allocator>"
      "Q24rstl180const_p",
      "");
  DoDemangleTest("test__FRCPCPCi", "test(const int* const* const&)");
  DoDemangleTest("__ct__Q34nw4r2ut14CharStrmReaderFMQ34nw4r2ut14CharStrmReaderFPCvPv_Us",
                 "nw4r::ut::CharStrmReader::CharStrmReader(unsigned short "
                 "(nw4r::ut::CharStrmReader::*)())");
  DoDemangleTest("QuerySymbolToMapFile___Q24nw4r2dbFPUcPC12OSModuleInfoUlPUcUl",
                 "nw4r::db::QuerySymbolToMapFile_(unsigned char*, const OSModuleInfo*, unsigned "
                 "long, unsigned char*, unsigned long)");
  DoDemangleTest("__ct__Q37JGadget27TLinkList<10JUTConsole,-24>"
                 "8iteratorFQ37JGadget13TNodeLinkList8iterator",
                 "JGadget::TLinkList<JUTConsole, "
                 "-24>::iterator::iterator(JGadget::TNodeLinkList::iterator)");
  DoDemangleTest(
      "do_assign<Q23std126__convert_iterator<PP16GAM_eEngineState,Q33std68__cdeque<P16GAM_"
      "eEngineState,36ubiSTLAllocator<P16GAM_eEngineState>>8iterator>>__Q23std36__cdeque<PCv,"
      "20ubiSTLAllocator<PCv>>FQ23std126__convert_iterator<PP16GAM_eEngineState,Q33std68__"
      "cdeque<P16GAM_eEngineState,36ubiSTLAllocator<P16GAM_eEngineState>>8iterator>Q23std126__"
      "convert_iterator<PP16GAM_eEngineState,Q33std68__cdeque<P16GAM_eEngineState,"
      "36ubiSTLAllocator<P16GAM_eEngineState>>8iterator>Q23std20forward_iterator_tag",
      "std::__cdeque<const void*, ubiSTLAllocator<const "
      "void*>>::do_assign<std::__convert_iterator<GAM_eEngineState**, "
      "std::__cdeque<GAM_eEngineState*, "
      "ubiSTLAllocator<GAM_eEngineState*>>::iterator>>(std::__convert_iterator<GAM_"
      "eEngineState**, std::__cdeque<GAM_eEngineState*, "
      "ubiSTLAllocator<GAM_eEngineState*>>::iterator>, "
      "std::__convert_iterator<GAM_eEngineState**, std::__cdeque<GAM_eEngineState*, "
      "ubiSTLAllocator<GAM_eEngineState*>>::iterator>, std::forward_iterator_tag)");
  DoDemangleTest(
      "__opPCQ23std15__locale_imp<1>__Q23std80_RefCountedPtr<Q23std15__locale_imp<1>,"
      "Q23std32_Single<Q23std15__locale_imp<1>>>CFv",
      "std::_RefCountedPtr<std::__locale_imp<1>, "
      "std::_Single<std::__locale_imp<1>>>::operator const std::__locale_imp<1>*() const");
  DoDemangleTest("__partition_const_ref<PP12CSpaceObject,Q23std74unary_negate<Q23std52__binder1st_"
                 "const_ref<Q23std21less<P12CSpaceObject>>>>__"
                 "3stdFPP12CSpaceObjectPP12CSpaceObjectRCQ23std74unary_negate<Q23std52__binder1st_"
                 "const_ref<Q23std21less<P12CSpaceObject>>>",
                 "std::__partition_const_ref<CSpaceObject**, "
                 "std::unary_negate<std::__binder1st_const_ref<std::less<CSpaceObject*>>>>("
                 "CSpaceObject**, CSpaceObject**, const "
                 "std::unary_negate<std::__binder1st_const_ref<std::less<CSpaceObject*>>>&)");
}

TEST(CWDemangler, TestDemangleOptions)
{
  DoDemangleOptionsTest(true, false, "__dt__26__partial_array_destructorFv",
                        "__partial_array_destructor::~__partial_array_destructor()");
  DoDemangleOptionsTest(false, false, "__dt__26__partial_array_destructorFv",
                        "__partial_array_destructor::~__partial_array_destructor(void)");
  DoDemangleOptionsTest(true, true,
                        "__opPCQ23std15__locale_imp<1>__Q23std80_RefCountedPtr<Q23std15__locale_"
                        "imp<1>,Q23std32_Single<Q23std15__locale_imp<1>>>CFv",
                        "std::_RefCountedPtr<std::__locale_imp<__int128>, "
                        "std::_Single<std::__locale_imp<__int128>>>::operator const "
                        "std::__locale_imp<__int128>*() const");
  DoDemangleOptionsTest(true, true, "fn<3,PV2>__FPC2",
                        "fn<3, volatile __vec2x32float__*>(const __vec2x32float__*)");
}

/*
 *
 * Copyright (c) 2002-2010, NVIDIA Corporation.
 *
 *
 *
 * NVIDIA Corporation("NVIDIA") supplies this software to you in consideration
 * of your agreement to the following terms, and your use, installation,
 * modification or redistribution of this NVIDIA software constitutes
 * acceptance of these terms.  If you do not agree with these terms, please do
 * not use, install, modify or redistribute this NVIDIA software.
 *
 *
 *
 * In consideration of your agreement to abide by the following terms, and
 * subject to these terms, NVIDIA grants you a personal, non-exclusive license,
 * under NVIDIA's copyrights in this original NVIDIA software (the "NVIDIA
 * Software"), to use, reproduce, modify and redistribute the NVIDIA
 * Software, with or without modifications, in source and/or binary forms;
 * provided that if you redistribute the NVIDIA Software, you must retain the
 * copyright notice of NVIDIA, this notice and the following text and
 * disclaimers in all such redistributions of the NVIDIA Software. Neither the
 * name, trademarks, service marks nor logos of NVIDIA Corporation may be used
 * to endorse or promote products derived from the NVIDIA Software without
 * specific prior written permission from NVIDIA.  Except as expressly stated
 * in this notice, no other rights or licenses express or implied, are granted
 * by NVIDIA herein, including but not limited to any patent rights that may be
 * infringed by your derivative works or by other works in which the NVIDIA
 * Software may be incorporated. No hardware is licensed hereunder.
 *
 *
 *
 * THE NVIDIA SOFTWARE IS BEING PROVIDED ON AN "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
 * WITHOUT LIMITATION, WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR ITS USE AND OPERATION
 * EITHER ALONE OR IN COMBINATION WITH OTHER PRODUCTS.
 *
 *
 *
 * IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL,
 * EXEMPLARY, CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, LOST
 * PROFITS; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) OR ARISING IN ANY WAY OUT OF THE USE,
 * REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE NVIDIA SOFTWARE,
 * HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING
 * NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF NVIDIA HAS BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#ifndef _cg_h
#define _cg_h

/*************************************************************************/
/*** CG Run-Time Library API                                          ***/
/*************************************************************************/

#define CG_VERSION_NUM                2200

#ifdef _WIN32
# ifndef APIENTRY /* From Win32's <windef.h> */
#  define CG_APIENTRY_DEFINED
#  if (_MSC_VER >= 800) || defined(_STDCALL_SUPPORTED) || defined(__BORLANDC__) || defined(__LCC__)
#   define APIENTRY    __stdcall
#  else
#   define APIENTRY
#  endif
# endif
# ifndef WINGDIAPI /* From Win32's <wingdi.h> and <winnt.h> */
#  define CG_WINGDIAPI_DEFINED
#  define WINGDIAPI __declspec(dllimport)
# endif
#endif /* _WIN32 */

/* Set up CG_API for Win32 dllexport or gcc visibility */

#ifndef CG_API
# ifdef CG_EXPORTS
#  ifdef _WIN32
#   define CG_API __declspec(dllexport)
#  elif defined(__GNUC__) && __GNUC__>=4
#   define CG_API __attribute__ ((visibility("default")))
#  elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#   define CG_API __global
#  else
#   define CG_API
#  endif
# else
#  define CG_API
# endif
#endif

#ifndef CGENTRY
# ifdef _WIN32
#  define CGENTRY __cdecl
# else
#  define CGENTRY
# endif
#endif

/*************************************************************************/
/*** Data types and enumerants                                         ***/
/*************************************************************************/

typedef int CGbool;

#define CG_FALSE ((CGbool)0)
#define CG_TRUE ((CGbool)1)

typedef struct _CGcontext *CGcontext;
typedef struct _CGprogram *CGprogram;
typedef struct _CGparameter *CGparameter;
typedef struct _CGobj *CGobj;
typedef struct _CGbuffer *CGbuffer;
typedef struct _CGeffect *CGeffect;
typedef struct _CGtechnique *CGtechnique;
typedef struct _CGpass *CGpass;
typedef struct _CGstate *CGstate;
typedef struct _CGstateassignment *CGstateassignment;
typedef struct _CGannotation *CGannotation;
typedef void *CGhandle;


/*!!! PREPROCESS BEGIN */

typedef enum
 {
  CG_UNKNOWN_TYPE,
  CG_STRUCT,
  CG_ARRAY,
  CG_TYPELESS_STRUCT,

  CG_TYPE_START_ENUM = 1024,
#define CG_DATATYPE_MACRO(name, compiler_name, enum_name, base_name, ncols, nrows, pc) \
  enum_name ,

#include <Cg/cg_datatypes.h>
#undef CG_DATATYPE_MACRO

  CG_TYPE_MAX
 } CGtype;

typedef enum
 {
# define CG_BINDLOCATION_MACRO(name,enum_name,compiler_name,\
                               enum_int,addressable,param_type) \
  enum_name = enum_int,

#include <Cg/cg_bindlocations.h>

  CG_UNDEFINED = 3256
 } CGresource;

typedef enum
 {
  CG_PROFILE_START = 6144,
  CG_PROFILE_UNKNOWN,

#define CG_PROFILE_MACRO(name, compiler_id, compiler_id_caps, compiler_opt,int_id,vertex_profile) \
  CG_PROFILE_##compiler_id_caps = int_id,
#define CG_PROFILE_ALIAS(name, compiler_id, compiler_id_caps, compiler_opt,int_id,vertex_profile) \
  CG_PROFILE_MACRO(name, compiler_id, compiler_id_caps, compiler_opt,int_id,vertex_profile)

#include <Cg/cg_profiles.h>

  CG_PROFILE_MAX = 7100
 } CGprofile;

typedef enum
 {
#define CG_ERROR_MACRO(code, enum_name, message) \
   enum_name = code,
#include <Cg/cg_errors.h>

  CG_ERROR_MAX
 } CGerror;

typedef enum
 {
#define CG_ENUM_MACRO(enum_name, enum_val) \
  enum_name = enum_val,
#include <Cg/cg_enums.h>

  CG_ENUM_MAX
 } CGenum;

/*!!! PREPROCESS END */

typedef enum
 {
  CG_PARAMETERCLASS_UNKNOWN = 0,
  CG_PARAMETERCLASS_SCALAR,
  CG_PARAMETERCLASS_VECTOR,
  CG_PARAMETERCLASS_MATRIX,
  CG_PARAMETERCLASS_STRUCT,
  CG_PARAMETERCLASS_ARRAY,
  CG_PARAMETERCLASS_SAMPLER,
  CG_PARAMETERCLASS_OBJECT,

  CG_PARAMETERCLASS_MAX
 } CGparameterclass;

typedef enum
{
  CG_UNKNOWN_DOMAIN = 0,
  CG_FIRST_DOMAIN   = 1,
  CG_VERTEX_DOMAIN  = 1,
  CG_FRAGMENT_DOMAIN,
  CG_GEOMETRY_DOMAIN,
  CG_NUMBER_OF_DOMAINS,

  CG_DOMAIN_MAX
} CGdomain;

typedef enum
{
  CG_MAP_READ = 0,
  CG_MAP_WRITE,
  CG_MAP_READ_WRITE,
  CG_MAP_WRITE_DISCARD,
  CG_MAP_WRITE_NO_OVERWRITE,

  CG_MAP_MAX
} CGbufferaccess;

typedef enum
{
  CG_BUFFER_USAGE_STREAM_DRAW = 0,
  CG_BUFFER_USAGE_STREAM_READ,
  CG_BUFFER_USAGE_STREAM_COPY,
  CG_BUFFER_USAGE_STATIC_DRAW,
  CG_BUFFER_USAGE_STATIC_READ,
  CG_BUFFER_USAGE_STATIC_COPY,
  CG_BUFFER_USAGE_DYNAMIC_DRAW,
  CG_BUFFER_USAGE_DYNAMIC_READ,
  CG_BUFFER_USAGE_DYNAMIC_COPY,

  CG_BUFFER_USAGE_MAX
} CGbufferusage;

#ifdef __cplusplus
extern "C" {
#endif

typedef CGbool (CGENTRY * CGstatecallback)(CGstateassignment);
typedef void (CGENTRY * CGerrorCallbackFunc)(void);
typedef void (CGENTRY * CGerrorHandlerFunc)(CGcontext ctx, CGerror err, void *data);
typedef void (CGENTRY * CGIncludeCallbackFunc)(CGcontext ctx, const char *filename);

/*************************************************************************/
/*** Functions                                                         ***/
/*************************************************************************/

#ifndef CG_EXPLICIT

/*** Library policy functions ***/

CG_API CGenum CGENTRY cgSetLockingPolicy(CGenum lockingPolicy);
CG_API CGenum CGENTRY cgGetLockingPolicy(void);
CG_API CGenum CGENTRY cgSetSemanticCasePolicy(CGenum casePolicy);
CG_API CGenum CGENTRY cgGetSemanticCasePolicy(void);

/*** Context functions ***/

CG_API CGcontext CGENTRY cgCreateContext(void);
CG_API void CGENTRY cgDestroyContext(CGcontext ctx);
CG_API CGbool CGENTRY cgIsContext(CGcontext ctx);
CG_API const char * CGENTRY cgGetLastListing(CGcontext ctx);
CG_API void CGENTRY cgSetLastListing(CGhandle handle, const char *listing);
CG_API void CGENTRY cgSetAutoCompile(CGcontext ctx, CGenum flag);
CG_API CGenum CGENTRY cgGetAutoCompile(CGcontext ctx);
CG_API void CGENTRY cgSetParameterSettingMode(CGcontext ctx, CGenum parameterSettingMode);
CG_API CGenum CGENTRY cgGetParameterSettingMode(CGcontext ctx);

/*** Inclusion ***/

CG_API void CGENTRY cgSetCompilerIncludeString(CGcontext ctx, const char *name, const char *source);
CG_API void CGENTRY cgSetCompilerIncludeFile(CGcontext ctx, const char *name, const char *filename);
CG_API void CGENTRY cgSetCompilerIncludeCallback(CGcontext ctx, CGIncludeCallbackFunc func);
CG_API CGIncludeCallbackFunc CGENTRY cgGetCompilerIncludeCallback(CGcontext ctx);

/*** Program functions ***/

CG_API CGprogram CGENTRY cgCreateProgram(CGcontext ctx,
                                    CGenum program_type,
                                    const char *program,
                                    CGprofile profile,
                                    const char *entry,
                                    const char **args);
CG_API CGprogram CGENTRY cgCreateProgramFromFile(CGcontext ctx,
                                            CGenum program_type,
                                            const char *program_file,
                                            CGprofile profile,
                                            const char *entry,
                                            const char **args);
CG_API CGprogram CGENTRY cgCopyProgram(CGprogram program);
CG_API void CGENTRY cgDestroyProgram(CGprogram program);

CG_API CGprogram CGENTRY cgGetFirstProgram(CGcontext ctx);
CG_API CGprogram CGENTRY cgGetNextProgram(CGprogram current);
CG_API CGcontext CGENTRY cgGetProgramContext(CGprogram prog);
CG_API CGbool CGENTRY cgIsProgram(CGprogram program);

CG_API void CGENTRY cgCompileProgram(CGprogram program);
CG_API CGbool CGENTRY cgIsProgramCompiled(CGprogram program);
CG_API const char * CGENTRY cgGetProgramString(CGprogram prog, CGenum pname);
CG_API CGprofile CGENTRY cgGetProgramProfile(CGprogram prog);
CG_API char const * const * CGENTRY cgGetProgramOptions(CGprogram prog);
CG_API void CGENTRY cgSetProgramProfile(CGprogram prog, CGprofile profile);
CG_API CGenum CGENTRY cgGetProgramInput(CGprogram program);
CG_API CGenum CGENTRY cgGetProgramOutput(CGprogram program);
CG_API void CGENTRY cgSetPassProgramParameters(CGprogram);
CG_API void CGENTRY cgUpdateProgramParameters(CGprogram program);
CG_API void CGENTRY cgUpdatePassParameters(CGpass pass);

/*** Parameter functions ***/

CG_API CGparameter CGENTRY cgCreateParameter(CGcontext ctx, CGtype type);
CG_API CGparameter CGENTRY cgCreateParameterArray(CGcontext ctx,
                                             CGtype type,
                                             int length);
CG_API CGparameter CGENTRY cgCreateParameterMultiDimArray(CGcontext ctx,
                                                     CGtype type,
                                                     int dim,
                                                     const int *lengths);
CG_API void CGENTRY cgDestroyParameter(CGparameter param);
CG_API void CGENTRY cgConnectParameter(CGparameter from, CGparameter to);
CG_API void CGENTRY cgDisconnectParameter(CGparameter param);
CG_API CGparameter CGENTRY cgGetConnectedParameter(CGparameter param);

CG_API int CGENTRY cgGetNumConnectedToParameters(CGparameter param);
CG_API CGparameter CGENTRY cgGetConnectedToParameter(CGparameter param, int index);

CG_API CGparameter CGENTRY cgGetNamedParameter(CGprogram prog, const char *name);
CG_API CGparameter CGENTRY cgGetNamedProgramParameter(CGprogram prog,
                                                 CGenum name_space,
                                                 const char *name);

CG_API CGparameter CGENTRY cgGetFirstParameter(CGprogram prog, CGenum name_space);
CG_API CGparameter CGENTRY cgGetNextParameter(CGparameter current);
CG_API CGparameter CGENTRY cgGetFirstLeafParameter(CGprogram prog, CGenum name_space);
CG_API CGparameter CGENTRY cgGetNextLeafParameter(CGparameter current);

CG_API CGparameter CGENTRY cgGetFirstStructParameter(CGparameter param);
CG_API CGparameter CGENTRY cgGetNamedStructParameter(CGparameter param,
                                                const char *name);

CG_API CGparameter CGENTRY cgGetFirstDependentParameter(CGparameter param);

CG_API CGparameter CGENTRY cgGetArrayParameter(CGparameter aparam, int index);
CG_API int CGENTRY cgGetArrayDimension(CGparameter param);
CG_API CGtype CGENTRY cgGetArrayType(CGparameter param);
CG_API int CGENTRY cgGetArraySize(CGparameter param, int dimension);
CG_API int CGENTRY cgGetArrayTotalSize(CGparameter param);
CG_API void CGENTRY cgSetArraySize(CGparameter param, int size);
CG_API void CGENTRY cgSetMultiDimArraySize(CGparameter param, const int *sizes);

CG_API CGprogram CGENTRY cgGetParameterProgram(CGparameter param);
CG_API CGcontext CGENTRY cgGetParameterContext(CGparameter param);
CG_API CGbool CGENTRY cgIsParameter(CGparameter param);
CG_API const char * CGENTRY cgGetParameterName(CGparameter param);
CG_API CGtype CGENTRY cgGetParameterType(CGparameter param);
CG_API CGtype CGENTRY cgGetParameterBaseType(CGparameter param);
CG_API CGparameterclass CGENTRY cgGetParameterClass(CGparameter param);
CG_API int CGENTRY cgGetParameterRows(CGparameter param);
CG_API int CGENTRY cgGetParameterColumns(CGparameter param);
CG_API CGtype CGENTRY cgGetParameterNamedType(CGparameter param);
CG_API const char * CGENTRY cgGetParameterSemantic(CGparameter param);
CG_API CGresource CGENTRY cgGetParameterResource(CGparameter param);
CG_API CGresource CGENTRY cgGetParameterBaseResource(CGparameter param);
CG_API unsigned long CGENTRY cgGetParameterResourceIndex(CGparameter param);
CG_API CGenum CGENTRY cgGetParameterVariability(CGparameter param);
CG_API CGenum CGENTRY cgGetParameterDirection(CGparameter param);
CG_API CGbool CGENTRY cgIsParameterReferenced(CGparameter param);
CG_API CGbool CGENTRY cgIsParameterUsed(CGparameter param, CGhandle handle);
CG_API const double * CGENTRY cgGetParameterValues(CGparameter param,
                                             CGenum value_type,
                                             int *nvalues);
CG_API void CGENTRY cgSetParameterValuedr(CGparameter param, int n, const double *vals);
CG_API void CGENTRY cgSetParameterValuedc(CGparameter param, int n, const double *vals);
CG_API void CGENTRY cgSetParameterValuefr(CGparameter param, int n, const float *vals);
CG_API void CGENTRY cgSetParameterValuefc(CGparameter param, int n, const float *vals);
CG_API void CGENTRY cgSetParameterValueir(CGparameter param, int n, const int *vals);
CG_API void CGENTRY cgSetParameterValueic(CGparameter param, int n, const int *vals);
CG_API int CGENTRY cgGetParameterValuedr(CGparameter param, int n, double *vals);
CG_API int CGENTRY cgGetParameterValuedc(CGparameter param, int n, double *vals);
CG_API int CGENTRY cgGetParameterValuefr(CGparameter param, int n, float *vals);
CG_API int CGENTRY cgGetParameterValuefc(CGparameter param, int n, float *vals);
CG_API int CGENTRY cgGetParameterValueir(CGparameter param, int n, int *vals);
CG_API int CGENTRY cgGetParameterValueic(CGparameter param, int n, int *vals);
CG_API int CGENTRY cgGetParameterDefaultValuedr(CGparameter param, int n, double *vals);
CG_API int CGENTRY cgGetParameterDefaultValuedc(CGparameter param, int n, double *vals);
CG_API int CGENTRY cgGetParameterDefaultValuefr(CGparameter param, int n, float *vals);
CG_API int CGENTRY cgGetParameterDefaultValuefc(CGparameter param, int n, float *vals);
CG_API int CGENTRY cgGetParameterDefaultValueir(CGparameter param, int n, int *vals);
CG_API int CGENTRY cgGetParameterDefaultValueic(CGparameter param, int n, int *vals);
CG_API const char * CGENTRY cgGetStringParameterValue(CGparameter param);
CG_API void CGENTRY cgSetStringParameterValue(CGparameter param, const char *str);

CG_API int CGENTRY cgGetParameterOrdinalNumber(CGparameter param);
CG_API CGbool CGENTRY cgIsParameterGlobal(CGparameter param);
CG_API int CGENTRY cgGetParameterIndex(CGparameter param);

CG_API void CGENTRY cgSetParameterVariability(CGparameter param, CGenum vary);
CG_API void CGENTRY cgSetParameterSemantic(CGparameter param, const char *semantic);

CG_API void CGENTRY cgSetParameter1f(CGparameter param, float x);
CG_API void CGENTRY cgSetParameter2f(CGparameter param, float x, float y);
CG_API void CGENTRY cgSetParameter3f(CGparameter param, float x, float y, float z);
CG_API void CGENTRY cgSetParameter4f(CGparameter param,
                                float x,
                                float y,
                                float z,
                                float w);
CG_API void CGENTRY cgSetParameter1d(CGparameter param, double x);
CG_API void CGENTRY cgSetParameter2d(CGparameter param, double x, double y);
CG_API void CGENTRY cgSetParameter3d(CGparameter param,
                                double x,
                                double y,
                                double z);
CG_API void CGENTRY cgSetParameter4d(CGparameter param,
                                double x,
                                double y,
                                double z,
                                double w);
CG_API void CGENTRY cgSetParameter1i(CGparameter param, int x);
CG_API void CGENTRY cgSetParameter2i(CGparameter param, int x, int y);
CG_API void CGENTRY cgSetParameter3i(CGparameter param, int x, int y, int z);
CG_API void CGENTRY cgSetParameter4i(CGparameter param,
                                int x,
                                int y,
                                int z,
                                int w);

CG_API void CGENTRY cgSetParameter1iv(CGparameter param, const int *v);
CG_API void CGENTRY cgSetParameter2iv(CGparameter param, const int *v);
CG_API void CGENTRY cgSetParameter3iv(CGparameter param, const int *v);
CG_API void CGENTRY cgSetParameter4iv(CGparameter param, const int *v);
CG_API void CGENTRY cgSetParameter1fv(CGparameter param, const float *v);
CG_API void CGENTRY cgSetParameter2fv(CGparameter param, const float *v);
CG_API void CGENTRY cgSetParameter3fv(CGparameter param, const float *v);
CG_API void CGENTRY cgSetParameter4fv(CGparameter param, const float *v);
CG_API void CGENTRY cgSetParameter1dv(CGparameter param, const double *v);
CG_API void CGENTRY cgSetParameter2dv(CGparameter param, const double *v);
CG_API void CGENTRY cgSetParameter3dv(CGparameter param, const double *v);
CG_API void CGENTRY cgSetParameter4dv(CGparameter param, const double *v);

CG_API void CGENTRY cgSetMatrixParameterir(CGparameter param, const int *matrix);
CG_API void CGENTRY cgSetMatrixParameterdr(CGparameter param, const double *matrix);
CG_API void CGENTRY cgSetMatrixParameterfr(CGparameter param, const float *matrix);
CG_API void CGENTRY cgSetMatrixParameteric(CGparameter param, const int *matrix);
CG_API void CGENTRY cgSetMatrixParameterdc(CGparameter param, const double *matrix);
CG_API void CGENTRY cgSetMatrixParameterfc(CGparameter param, const float *matrix);

CG_API void CGENTRY cgGetMatrixParameterir(CGparameter param, int *matrix);
CG_API void CGENTRY cgGetMatrixParameterdr(CGparameter param, double *matrix);
CG_API void CGENTRY cgGetMatrixParameterfr(CGparameter param, float *matrix);
CG_API void CGENTRY cgGetMatrixParameteric(CGparameter param, int *matrix);
CG_API void CGENTRY cgGetMatrixParameterdc(CGparameter param, double *matrix);
CG_API void CGENTRY cgGetMatrixParameterfc(CGparameter param, float *matrix);

CG_API CGenum CGENTRY cgGetMatrixParameterOrder(CGparameter param);

CG_API CGparameter CGENTRY cgGetNamedSubParameter(CGparameter param, const char *name);

/*** Type Functions ***/

CG_API const char * CGENTRY cgGetTypeString(CGtype type);
CG_API CGtype CGENTRY cgGetType(const char *type_string);

CG_API CGtype CGENTRY cgGetNamedUserType(CGhandle handle, const char *name);

CG_API int CGENTRY cgGetNumUserTypes(CGhandle handle);
CG_API CGtype CGENTRY cgGetUserType(CGhandle handle, int index);

CG_API int CGENTRY cgGetNumParentTypes(CGtype type);
CG_API CGtype CGENTRY cgGetParentType(CGtype type, int index);

CG_API CGbool CGENTRY cgIsParentType(CGtype parent, CGtype child);
CG_API CGbool CGENTRY cgIsInterfaceType(CGtype type);

/*** Resource Functions ***/

CG_API const char * CGENTRY cgGetResourceString(CGresource resource);
CG_API CGresource CGENTRY cgGetResource(const char *resource_string);

/*** Enum Functions ***/

CG_API const char * CGENTRY cgGetEnumString(CGenum en);
CG_API CGenum CGENTRY cgGetEnum(const char *enum_string);

/*** Profile Functions ***/

CG_API const char * CGENTRY cgGetProfileString(CGprofile profile);
CG_API CGprofile CGENTRY cgGetProfile(const char *profile_string);
CG_API int CGENTRY cgGetNumSupportedProfiles(void);
CG_API CGprofile CGENTRY cgGetSupportedProfile(int index);
CG_API CGbool CGENTRY cgIsProfileSupported(CGprofile profile);
CG_API CGbool CGENTRY cgGetProfileProperty(CGprofile profile, CGenum query);

/*** ParameterClass Functions ***/

CG_API const char * CGENTRY cgGetParameterClassString(CGparameterclass pc);
CG_API CGparameterclass CGENTRY cgGetParameterClassEnum(const char * pString);

/*** Domain Functions ***/

CG_API const char * CGENTRY cgGetDomainString(CGdomain domain);
CG_API CGdomain CGENTRY cgGetDomain(const char *domain_string);
CG_API CGdomain CGENTRY cgGetProgramDomain(CGprogram program);

/*** Error Functions ***/

CG_API CGerror CGENTRY cgGetError(void);
CG_API CGerror CGENTRY cgGetFirstError(void);
CG_API const char * CGENTRY cgGetErrorString(CGerror error);
CG_API const char * CGENTRY cgGetLastErrorString(CGerror *error);
CG_API void CGENTRY cgSetErrorCallback(CGerrorCallbackFunc func);
CG_API CGerrorCallbackFunc CGENTRY cgGetErrorCallback(void);
CG_API void CGENTRY cgSetErrorHandler(CGerrorHandlerFunc func, void *data);
CG_API CGerrorHandlerFunc CGENTRY cgGetErrorHandler(void **data);

/*** Misc Functions ***/

CG_API const char * CGENTRY cgGetString(CGenum sname);


/*** CgFX Functions ***/

CG_API CGeffect CGENTRY cgCreateEffect(CGcontext, const char *code, const char **args);
CG_API CGeffect CGENTRY cgCreateEffectFromFile(CGcontext, const char *filename,
                                                 const char **args);
CG_API CGeffect CGENTRY cgCopyEffect(CGeffect effect);
CG_API void CGENTRY cgDestroyEffect(CGeffect);
CG_API CGcontext CGENTRY cgGetEffectContext(CGeffect);
CG_API CGbool CGENTRY cgIsEffect(CGeffect effect);

CG_API CGeffect CGENTRY cgGetFirstEffect(CGcontext);
CG_API CGeffect CGENTRY cgGetNextEffect(CGeffect);

CG_API CGprogram CGENTRY cgCreateProgramFromEffect(CGeffect effect,
                                                     CGprofile profile,
                                                     const char *entry,
                                                     const char **args);

CG_API CGtechnique CGENTRY cgGetFirstTechnique(CGeffect);
CG_API CGtechnique CGENTRY cgGetNextTechnique(CGtechnique);
CG_API CGtechnique CGENTRY cgGetNamedTechnique(CGeffect, const char *name);
CG_API const char * CGENTRY cgGetTechniqueName(CGtechnique);
CG_API CGbool CGENTRY cgIsTechnique(CGtechnique);
CG_API CGbool CGENTRY cgValidateTechnique(CGtechnique);
CG_API CGbool CGENTRY cgIsTechniqueValidated(CGtechnique);
CG_API CGeffect CGENTRY cgGetTechniqueEffect(CGtechnique);

CG_API CGpass CGENTRY cgGetFirstPass(CGtechnique);
CG_API CGpass CGENTRY cgGetNamedPass(CGtechnique, const char *name);
CG_API CGpass CGENTRY cgGetNextPass(CGpass);
CG_API CGbool CGENTRY cgIsPass(CGpass);
CG_API const char * CGENTRY cgGetPassName(CGpass);
CG_API CGtechnique CGENTRY cgGetPassTechnique(CGpass);
CG_API CGprogram CGENTRY cgGetPassProgram(CGpass pass, CGdomain domain);

CG_API void CGENTRY cgSetPassState(CGpass);
CG_API void CGENTRY cgResetPassState(CGpass);

CG_API CGstateassignment CGENTRY cgGetFirstStateAssignment(CGpass);
CG_API CGstateassignment CGENTRY cgGetNamedStateAssignment(CGpass, const char *name);
CG_API CGstateassignment CGENTRY cgGetNextStateAssignment(CGstateassignment);
CG_API CGbool CGENTRY cgIsStateAssignment(CGstateassignment);
CG_API CGbool CGENTRY cgCallStateSetCallback(CGstateassignment);
CG_API CGbool CGENTRY cgCallStateValidateCallback(CGstateassignment);
CG_API CGbool CGENTRY cgCallStateResetCallback(CGstateassignment);
CG_API CGpass CGENTRY cgGetStateAssignmentPass(CGstateassignment);
CG_API CGparameter CGENTRY cgGetSamplerStateAssignmentParameter(CGstateassignment);

CG_API const float * CGENTRY cgGetFloatStateAssignmentValues(CGstateassignment, int *nVals);
CG_API const int * CGENTRY cgGetIntStateAssignmentValues(CGstateassignment, int *nVals);
CG_API const CGbool * CGENTRY cgGetBoolStateAssignmentValues(CGstateassignment, int *nVals);
CG_API const char * CGENTRY cgGetStringStateAssignmentValue(CGstateassignment);
CG_API CGprogram CGENTRY cgGetProgramStateAssignmentValue(CGstateassignment);
CG_API CGparameter CGENTRY cgGetTextureStateAssignmentValue(CGstateassignment);
CG_API CGparameter CGENTRY cgGetSamplerStateAssignmentValue(CGstateassignment);
CG_API int CGENTRY cgGetStateAssignmentIndex(CGstateassignment);

CG_API int CGENTRY cgGetNumDependentStateAssignmentParameters(CGstateassignment);
CG_API CGparameter CGENTRY cgGetDependentStateAssignmentParameter(CGstateassignment, int index);

CG_API CGparameter CGENTRY cgGetConnectedStateAssignmentParameter(CGstateassignment);

CG_API CGstate CGENTRY cgGetStateAssignmentState(CGstateassignment);
CG_API CGstate CGENTRY cgGetSamplerStateAssignmentState(CGstateassignment);

CG_API CGstate CGENTRY cgCreateState(CGcontext, const char *name, CGtype);
CG_API CGstate CGENTRY cgCreateArrayState(CGcontext, const char *name, CGtype, int nelems);
CG_API void CGENTRY cgSetStateCallbacks(CGstate, CGstatecallback set, CGstatecallback reset,
                                   CGstatecallback validate);
CG_API void CGENTRY cgSetStateLatestProfile(CGstate, CGprofile);
CG_API CGstatecallback CGENTRY cgGetStateSetCallback(CGstate);
CG_API CGstatecallback CGENTRY cgGetStateResetCallback(CGstate);
CG_API CGstatecallback CGENTRY cgGetStateValidateCallback(CGstate);
CG_API CGprofile CGENTRY cgGetStateLatestProfile(CGstate);
CG_API CGcontext CGENTRY cgGetStateContext(CGstate);
CG_API CGtype CGENTRY cgGetStateType(CGstate);
CG_API const char * CGENTRY cgGetStateName(CGstate);
CG_API CGstate CGENTRY cgGetNamedState(CGcontext, const char *name);
CG_API CGstate CGENTRY cgGetFirstState(CGcontext);
CG_API CGstate CGENTRY cgGetNextState(CGstate);
CG_API CGbool CGENTRY cgIsState(CGstate);
CG_API void CGENTRY cgAddStateEnumerant(CGstate, const char *name, int value);

CG_API CGstate CGENTRY cgCreateSamplerState(CGcontext, const char *name, CGtype);
CG_API CGstate CGENTRY cgCreateArraySamplerState(CGcontext, const char *name, CGtype, int nelems);
CG_API CGstate CGENTRY cgGetNamedSamplerState(CGcontext, const char *name);
CG_API CGstate CGENTRY cgGetFirstSamplerState(CGcontext);

CG_API CGstateassignment CGENTRY cgGetFirstSamplerStateAssignment(CGparameter);
CG_API CGstateassignment CGENTRY cgGetNamedSamplerStateAssignment(CGparameter, const char *);
CG_API void CGENTRY cgSetSamplerState(CGparameter);

CG_API CGparameter CGENTRY cgGetNamedEffectParameter(CGeffect, const char *);
CG_API CGparameter CGENTRY cgGetFirstLeafEffectParameter(CGeffect);
CG_API CGparameter CGENTRY cgGetFirstEffectParameter(CGeffect);
CG_API CGparameter CGENTRY cgGetEffectParameterBySemantic(CGeffect, const char *);

CG_API CGannotation CGENTRY cgGetFirstTechniqueAnnotation(CGtechnique);
CG_API CGannotation CGENTRY cgGetFirstPassAnnotation(CGpass);
CG_API CGannotation CGENTRY cgGetFirstParameterAnnotation(CGparameter);
CG_API CGannotation CGENTRY cgGetFirstProgramAnnotation(CGprogram);
CG_API CGannotation CGENTRY cgGetFirstEffectAnnotation(CGeffect);
CG_API CGannotation CGENTRY cgGetNextAnnotation(CGannotation);

CG_API CGannotation CGENTRY cgGetNamedTechniqueAnnotation(CGtechnique, const char *);
CG_API CGannotation CGENTRY cgGetNamedPassAnnotation(CGpass, const char *);
CG_API CGannotation CGENTRY cgGetNamedParameterAnnotation(CGparameter, const char *);
CG_API CGannotation CGENTRY cgGetNamedProgramAnnotation(CGprogram, const char *);
CG_API CGannotation CGENTRY cgGetNamedEffectAnnotation(CGeffect, const char *);

CG_API CGbool CGENTRY cgIsAnnotation(CGannotation);

CG_API const char * CGENTRY cgGetAnnotationName(CGannotation);
CG_API CGtype CGENTRY cgGetAnnotationType(CGannotation);

CG_API const float * CGENTRY cgGetFloatAnnotationValues(CGannotation, int *nvalues);
CG_API const int * CGENTRY cgGetIntAnnotationValues(CGannotation, int *nvalues);
CG_API const char * CGENTRY cgGetStringAnnotationValue(CGannotation);
CG_API const char * const * CGENTRY cgGetStringAnnotationValues(CGannotation, int *nvalues);
CG_API const CGbool * CGENTRY cgGetBoolAnnotationValues(CGannotation, int *nvalues);
CG_API const int * CGENTRY cgGetBooleanAnnotationValues(CGannotation, int *nvalues);

CG_API int CGENTRY cgGetNumDependentAnnotationParameters(CGannotation);
CG_API CGparameter CGENTRY cgGetDependentAnnotationParameter(CGannotation, int index);

CG_API void CGENTRY cgEvaluateProgram(CGprogram, float *, int ncomps, int nx, int ny, int nz);

/*** Cg 1.5 Additions ***/

CG_API CGbool CGENTRY cgSetEffectName(CGeffect, const char *name);
CG_API const char * CGENTRY cgGetEffectName(CGeffect);
CG_API CGeffect CGENTRY cgGetNamedEffect(CGcontext, const char *name);
CG_API CGparameter CGENTRY cgCreateEffectParameter(CGeffect, const char *name, CGtype);

CG_API CGtechnique CGENTRY cgCreateTechnique(CGeffect, const char *name);

CG_API CGparameter CGENTRY cgCreateEffectParameterArray(CGeffect, const char *name, CGtype type, int length);
CG_API CGparameter CGENTRY cgCreateEffectParameterMultiDimArray(CGeffect, const char *name, CGtype type, int dim, const int *lengths);

CG_API CGpass CGENTRY cgCreatePass(CGtechnique, const char *name);

CG_API CGstateassignment CGENTRY cgCreateStateAssignment(CGpass, CGstate);
CG_API CGstateassignment CGENTRY cgCreateStateAssignmentIndex(CGpass, CGstate, int index);
CG_API CGstateassignment CGENTRY cgCreateSamplerStateAssignment(CGparameter, CGstate);

CG_API CGbool CGENTRY cgSetFloatStateAssignment(CGstateassignment, float);
CG_API CGbool CGENTRY cgSetIntStateAssignment(CGstateassignment, int);
CG_API CGbool CGENTRY cgSetBoolStateAssignment(CGstateassignment, CGbool);
CG_API CGbool CGENTRY cgSetStringStateAssignment(CGstateassignment, const char *);
CG_API CGbool CGENTRY cgSetProgramStateAssignment(CGstateassignment, CGprogram);
CG_API CGbool CGENTRY cgSetSamplerStateAssignment(CGstateassignment, CGparameter);
CG_API CGbool CGENTRY cgSetTextureStateAssignment(CGstateassignment, CGparameter);

CG_API CGbool CGENTRY cgSetFloatArrayStateAssignment(CGstateassignment, const float *vals);
CG_API CGbool CGENTRY cgSetIntArrayStateAssignment(CGstateassignment, const int *vals);
CG_API CGbool CGENTRY cgSetBoolArrayStateAssignment(CGstateassignment, const CGbool *vals);

CG_API CGannotation CGENTRY cgCreateTechniqueAnnotation(CGtechnique, const char *name, CGtype);
CG_API CGannotation CGENTRY cgCreatePassAnnotation(CGpass, const char *name, CGtype);
CG_API CGannotation CGENTRY cgCreateParameterAnnotation(CGparameter, const char *name, CGtype);
CG_API CGannotation CGENTRY cgCreateProgramAnnotation(CGprogram, const char *name, CGtype);
CG_API CGannotation CGENTRY cgCreateEffectAnnotation(CGeffect, const char *name, CGtype);

CG_API CGbool CGENTRY cgSetIntAnnotation(CGannotation, int value);
CG_API CGbool CGENTRY cgSetFloatAnnotation(CGannotation, float value);
CG_API CGbool CGENTRY cgSetBoolAnnotation(CGannotation, CGbool value);
CG_API CGbool CGENTRY cgSetStringAnnotation(CGannotation, const char *value);

CG_API int CGENTRY cgGetNumStateEnumerants(CGstate);
CG_API const char * CGENTRY cgGetStateEnumerant(CGstate, int index, int* value);
CG_API const char * CGENTRY cgGetStateEnumerantName(CGstate, int value);
CG_API int CGENTRY cgGetStateEnumerantValue(CGstate, const char *name);

CG_API CGeffect CGENTRY cgGetParameterEffect(CGparameter param);

CG_API CGparameterclass CGENTRY cgGetTypeClass(CGtype type);
CG_API CGtype CGENTRY cgGetTypeBase(CGtype type);
CG_API CGbool CGENTRY cgGetTypeSizes(CGtype type, int *nrows, int *ncols);
CG_API void CGENTRY cgGetMatrixSize(CGtype type, int *nrows, int *ncols);

CG_API int CGENTRY cgGetNumProgramDomains( CGprogram program );
CG_API CGdomain CGENTRY cgGetProfileDomain( CGprofile profile );
CG_API CGprogram CGENTRY cgCombinePrograms( int n, const CGprogram *exeList );
CG_API CGprogram CGENTRY cgCombinePrograms2( const CGprogram exe1, const CGprogram exe2 );
CG_API CGprogram CGENTRY cgCombinePrograms3( const CGprogram exe1, const CGprogram exe2, const CGprogram exe3 );
CG_API CGprofile CGENTRY cgGetProgramDomainProfile(CGprogram program, int index);
CG_API CGprogram CGENTRY cgGetProgramDomainProgram(CGprogram program, int index);

/*** CGobj Functions ***/
CG_API CGobj CGENTRY cgCreateObj( CGcontext context, CGenum program_type, const char *source, CGprofile profile, const char **args );
CG_API CGobj CGENTRY cgCreateObjFromFile( CGcontext context, CGenum program_type, const char *source_file, CGprofile profile, const char **args );
CG_API void CGENTRY cgDestroyObj( CGobj obj );

CG_API long CGENTRY cgGetParameterResourceSize(CGparameter);
CG_API CGtype CGENTRY cgGetParameterResourceType(CGparameter);
CG_API const char* CGENTRY cgGetParameterResourceName(CGparameter param);
CG_API int CGENTRY cgGetParameterBufferIndex(CGparameter);
CG_API int CGENTRY cgGetParameterBufferOffset(CGparameter);

CG_API CGbuffer CGENTRY cgCreateBuffer(CGcontext, int size, const void *data, CGbufferusage bufferUsage);
CG_API void CGENTRY cgSetBufferData(CGbuffer, int size, const void *data);
CG_API void CGENTRY cgSetBufferSubData(CGbuffer, int offset, int size, const void *data);
CG_API void CGENTRY cgSetProgramBuffer(CGprogram program, int bufferIndex, CGbuffer buffer);

CG_API void * CGENTRY cgMapBuffer(CGbuffer buffer, CGbufferaccess access);
CG_API void CGENTRY cgUnmapBuffer(CGbuffer buffer);
CG_API void CGENTRY cgDestroyBuffer(CGbuffer buffer);
CG_API CGbuffer CGENTRY cgGetProgramBuffer(CGprogram, int bufferIndex);
CG_API int CGENTRY cgGetBufferSize(CGbuffer);
CG_API int CGENTRY cgGetProgramBufferMaxSize(CGprofile profile);
CG_API int CGENTRY cgGetProgramBufferMaxIndex(CGprofile profile);

#endif

#ifdef __cplusplus
}
#endif

#ifdef CG_APIENTRY_DEFINED
# undef CG_APIENTRY_DEFINED
# undef APIENTRY
#endif

#ifdef CG_WINGDIAPI_DEFINED
# undef CG_WINGDIAPI_DEFINED
# undef WINGDIAPI
#endif

#endif

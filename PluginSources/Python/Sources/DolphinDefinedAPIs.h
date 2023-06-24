#ifndef DOLPHIN_DEFINED_APIS
#define DOLPHIN_DEFINED_APIS

#include "CopiedScriptContextFiles/InternalScriptAPIs/ArgHolder_APIs.h"
#include "CopiedScriptContextFiles/InternalScriptAPIs/ClassFunctionsResolver_APIs.h"
#include "CopiedScriptContextFiles/InternalScriptAPIs/ClassMetadata_APIs.h"
#include "CopiedScriptContextFiles/InternalScriptAPIs/FileUtility_APIs.h"
#include "CopiedScriptContextFiles/InternalScriptAPIs/FunctionMetadata_APIs.h"
#include "CopiedScriptContextFiles/InternalScriptAPIs/GCButton_APIs.h"
#include "CopiedScriptContextFiles/InternalScriptAPIs/ModuleLists_APIs.h"
#include "CopiedScriptContextFiles/InternalScriptAPIs/ScriptContext_APIs.h"
#include "CopiedScriptContextFiles/InternalScriptAPIs/VectorOfArgHolders_APIs.h"


#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#define DLL_Export   __declspec( dllexport )
#else
#define DLL_Export
#endif

extern ArgHolder_APIs argHolder_APIs;
extern ClassFunctionsResolver_APIs classFunctionsResolver_APIs;
extern ClassMetadata_APIs classMetadata_APIs;
extern FileUtility_APIs fileUtility_APIs;
extern FunctionMetadata_APIs functionMetadata_APIs;
extern GCButton_APIs gcButton_APIs;
extern ModuleLists_APIs moduleLists_APIs;
extern Dolphin_Defined_ScriptContext_APIs dolphinDefinedScriptContext_APIs;
extern VectorOfArgHolders_APIs vectorOfArgHolder_APIs;

DLL_Export void Init_ArgHolder_APIs(ArgHolder_APIs*);
DLL_Export void Init_ClassFunctionsResolver_APIs(ClassFunctionsResolver_APIs*);
DLL_Export void Init_ClassMetadata_APIs(ClassMetadata_APIs*);
DLL_Export void Init_FileUtility_APIs(FileUtility_APIs*);
DLL_Export void Init_FunctionMetadata_APIs(FunctionMetadata_APIs*);
DLL_Export void Init_GCButton_APIs(GCButton_APIs*);
DLL_Export void Init_ModuleLists_APIs(ModuleLists_APIs*);
DLL_Export void Init_ScriptContext_APIs(Dolphin_Defined_ScriptContext_APIs*);
DLL_Export void Init_VectorOfArgHolders_APIs(VectorOfArgHolders_APIs*);

#ifdef __cplusplus
}
#endif

#endif

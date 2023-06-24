#include "DolphinDefinedAPIs.h"
 
 ArgHolder_APIs argHolder_APIs;
 ClassFunctionsResolver_APIs classFunctionsResolver_APIs;
 ClassMetadata_APIs classMetadata_APIs;
 FileUtility_APIs fileUtility_APIs;
 FunctionMetadata_APIs functionMetadata_APIs;
 GCButton_APIs gcButton_APIs;
 ModuleLists_APIs moduleLists_APIs;
 Dolphin_Defined_ScriptContext_APIs dolphinDefinedScriptContext_APIs;
 VectorOfArgHolders_APIs vectorOfArgHolder_APIs;

DLL_Export void Init_ArgHolder_APIs(ArgHolder_APIs* new_argHolder_APIs)
{
  argHolder_APIs = *new_argHolder_APIs;
}

DLL_Export void Init_ClassFunctionsResolver_APIs(ClassFunctionsResolver_APIs* new_classFunctionsResolver_APIs)
{
  classFunctionsResolver_APIs = *new_classFunctionsResolver_APIs;
}

DLL_Export void Init_ClassMetadata_APIs(ClassMetadata_APIs* new_classMetadata_APIs)
{
  classMetadata_APIs = *new_classMetadata_APIs;
}

DLL_Export void Init_FileUtility_APIs(FileUtility_APIs* new_fileUtility_APIs)
{
  fileUtility_APIs = *new_fileUtility_APIs;
}

DLL_Export void Init_FunctionMetadata_APIs(FunctionMetadata_APIs* new_functionMetadata_APIs)
{
  functionMetadata_APIs = *new_functionMetadata_APIs;
}

DLL_Export void Init_GCButton_APIs(GCButton_APIs* new_gcButton_APIs)
{
  gcButton_APIs = *new_gcButton_APIs;
}

DLL_Export void Init_ModuleLists_APIs(ModuleLists_APIs* new_moduleLists_APIs)
{
	moduleLists_APIs = *new_moduleLists_APIs;
}

DLL_Export void Init_ScriptContext_APIs(Dolphin_Defined_ScriptContext_APIs* new_dolphinDefinedScriptContext_APIs)
{
  dolphinDefinedScriptContext_APIs = *new_dolphinDefinedScriptContext_APIs;
}

DLL_Export void Init_VectorOfArgHolders_APIs(VectorOfArgHolders_APIs* new_vectorOfArgHolders_APIs)
{
  vectorOfArgHolder_APIs = *new_vectorOfArgHolders_APIs;
}

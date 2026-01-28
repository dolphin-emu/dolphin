#include "Core/Scripting/ScriptUtilities.h"

#include "Core/Scripting/HelperClasses/InternalAPIDefinitions/FileUtility_API_Implementations.h"

namespace Scripting
{

static bool is_scripting_core_initialized = false;

void InitScriptingCore()
{
  if (!is_scripting_core_initialized)
  {
    FileUtilityAPIsImpl::Init();
    is_scripting_core_initialized = true;
  }
}

}  // namespace Scripting

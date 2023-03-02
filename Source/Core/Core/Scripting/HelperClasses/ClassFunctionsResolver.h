#pragma once
#include <string>
#include "Core/Scripting/HelperClasses/ClassMetadata.h"

namespace Scripting
{
ClassMetadata GetClassMetadataForModule(const std::string& module_name, const std::string& version_number);
}

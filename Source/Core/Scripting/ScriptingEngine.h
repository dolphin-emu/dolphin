#pragma once

#include <filesystem>

namespace Scripting
{

void Init(std::filesystem::path script_filepath);
void Shutdown();

} // namespace Scripting

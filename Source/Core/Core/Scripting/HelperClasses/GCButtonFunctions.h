#include "Core/Scripting/CoreScriptContextFiles/Enums/GCButtonNameEnum.h"
#include <vector>
#include "string.h"


extern GcButtonNameEnum ParseGCButton(const char* button_name);
extern const char* ConvertButtonEnumToString(GcButtonNameEnum button);
extern std::vector<GcButtonNameEnum> GetListOfAllButtons();

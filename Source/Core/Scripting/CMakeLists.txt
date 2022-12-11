add_library(scripting
  ScriptingEngine.cpp
  ScriptingEngine.h
  Python/coroutine.cpp
  Python/coroutine.h
  Python/PyScriptingBackend.cpp
  Python/PyScriptingBackend.h
  Python/Modules/controllermodule.cpp
  Python/Modules/controllermodule.h
  Python/Modules/doliomodule.cpp
  Python/Modules/doliomodule.h
  Python/Modules/dolphinmodule.cpp
  Python/Modules/dolphinmodule.h
  Python/Modules/eventmodule.cpp
  Python/Modules/eventmodule.h
  Python/Modules/guimodule.cpp
  Python/Modules/guimodule.h
  Python/Modules/memorymodule.cpp
  Python/Modules/memorymodule.h
  Python/Modules/registersmodule.cpp
  Python/Modules/registersmodule.h
  Python/Modules/savestatemodule.cpp
  Python/Modules/savestatemodule.h
  Python/Utils/as_py_func.h
  Python/Utils/convert.h
  Python/Utils/fmt.h
  Python/Utils/invoke.h
  Python/Utils/module.h
  Python/Utils/object_wrapper.cpp
  Python/Utils/object_wrapper.h
)

find_package(Python3 REQUIRED COMPONENTS Development)
include_directories(${Python3_INCLUDE_DIRS})
# TODO imgui shouldn't be here
target_link_libraries(scripting PRIVATE ${Python3_LIBRARIES} imgui core)

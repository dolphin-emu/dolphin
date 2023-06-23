#pragma once
#include <string>
#include <vector>
#include "FunctionMetadata.h"

class ClassMetadata
{
public:

  ClassMetadata()
  {
    class_name = "";
    functions_list = std::vector<FunctionMetadata>();
  }

  ClassMetadata(std::string new_class_name, std::vector<FunctionMetadata> new_functions_list)
  {
    class_name = new_class_name;
    functions_list = new_functions_list;
  }

  std::string class_name;
  std::vector<FunctionMetadata> functions_list;
};

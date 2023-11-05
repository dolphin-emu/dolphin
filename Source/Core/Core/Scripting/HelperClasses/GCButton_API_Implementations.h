#pragma once

// This file contains the implementations for the APIs in GCButton_APIs
namespace Scripting
{

int ParseGCButton_impl(const char* button_name);
const char* ConvertButtonEnumToString_impl(int button);
int IsValidButtonEnum_impl(int);
int IsDigitalButton_impl(int);
int IsAnalogButton_impl(int);

}  // namespace Scripting

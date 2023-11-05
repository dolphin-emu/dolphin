#pragma once

namespace Scripting
{

#ifdef __cplusplus
extern "C" {
#endif

// GCButton_APIs contains the APIs for handling GCButtonNameEnum values.
typedef struct GCButton_APIs
{
  // Takes as input a string (case-insensitive) representing a button name, and returns the int that
  // corresponds to the GCButtonNameEnum value representing the button.
  int (*ParseGCButton)(const char*);

  // Takes as input an integer representing a GCButtonNameEnum value, and returns the const char*
  // which represents the button name (if the return value for this function is passed into the
  // ParseGCButton function above, it will output the original input to this function).
  const char* (*ConvertButtonEnumToString)(int);

  // Returns 1 if the integer passed into it is a valid GCButtonNameEnum value, and returns 0
  // otherwise (returns 0 when it encounters GCButtonNameEnum::UnknownButton, or any value not lised
  // in the GCButtonNameEnum.h file)
  int (*IsValidButtonEnum)(int);

  // Returns 1 if the integer passed into it (representing a GCButtonNameEnum) represents a digital
  // button, and returns 0 otherwise
  int (*IsDigitalButton)(int);

  // Returns 1 if the integer passed into it (representing a GCButtonNameEnum) represents an analog
  // button, and returns 0 otherwise
  int (*IsAnalogButton)(int);

} GCButton_APIs;

#ifdef __cplusplus
}
#endif

}  // namespace Scripting

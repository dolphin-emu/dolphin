#ifndef GC_BUTTON_APIS
#define GC_BUTTON_APIS

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GCButton_APIs
{
  int (*ParseGCButton)(const char*);  // Takes as input a string (case-insensitive) representing a
                                      // button name, and returns the int that corresponds to the
                                      // GCButtonNameEnum value representing the button.
  const char* (*ConvertButtonEnumToString)(
      int);  // Takes as input an integer representing a GCButtonNameEnum value, and returns the
             // const char* which represents the button name (if the return value for this function
             // is passed into the function above, it will output the original input to this
             // function).
  int (*IsValidButtonEnum)(
      int);  // Returns 1 if the integer passed into it is a valid GCButtonNameEnum value, and
             // returns 0 otherwise (returns 0 when it encounters GCButtonNameEnum::UnknownButton,
             // or any value not lised in the GCButtonNameEnum.h file)
  int (*IsDigitalButton)(int);  // Returns 1 if the integer passed into it represents a digital
                                // button (for the GCButtonNameEnu) and returns 0 otherwise
  int (*IsAnalogButton)(int);   // Returns 1 if the integer passed into it represents an analog
                                // button (for the GCButtonNameENum), and returns 0 otherwise.
} GCButton_APIs;

#ifdef __cplusplus
}
#endif

#endif

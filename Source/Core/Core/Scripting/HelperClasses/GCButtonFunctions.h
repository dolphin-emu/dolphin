#ifndef GC_BUTTONS_FUNCS
#define GC_BUTTONS_FUNCS

#ifdef __cplusplus
extern "C" {
#endif

int ParseGCButton_impl(const char* button_name);
const char* ConvertButtonEnumToString_impl(int button);
int IsValidButtonEnum_impl(int);
int IsDigitalButton_impl(int);
int IsAnalogButton_impl(int);

#ifdef __cplusplus
}
#endif

#endif

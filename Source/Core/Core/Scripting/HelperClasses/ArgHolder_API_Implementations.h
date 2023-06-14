#ifndef ARG_HOLDER_API_IMPLS
#define ARG_HOLDER_API_IMPLS

namespace Scripting
{
#ifdef __cplusplus
extern "C" {
#endif

// Most files have their function APIs listed in the same file as their class is defined is.
// However, because ArgHolder has so many helper functions and so many APIs, I've put it into
// this seperate file here (all the other APIs are located in the file where their class
// definition is)

int GetArgType_ArgHolder_impl(void*);
int GetIsEmpty_ArgHolder_impl(void*);
void* CreateBoolArgHolder_API_impl(int);
void* CreateU8ArgHolder_API_impl(unsigned long long);
void* CreateU16ArgHolder_API_impl(unsigned long long);
void* CreateU32ArgHolder_API_impl(unsigned long long);
void* CreateU64ArgHolder_API_impl(unsigned long long);
void* CreateS8ArgHolder_API_impl(signed long long);
void* CreateS16ArgHolder_API_impl(signed long long);
void* CreateS32ArgHolder_API_impl(signed long long);
void* CreateS64ArgHolder_API_impl(signed long long);
void* CreateFloatArgHolder_API_impl(double);
void* CreateDoubleArgHolder_API_impl(double);
void* CreateStringArgHolder_API_impl(const char*);
void* CreateVoidPointerArgHolder_API_impl(void*);

void* CreateAddressToByteMapArgHolder_API_impl();

void AddPairToAddressToByteMapArgHolder_impl(void*, long long, signed int);
unsigned int GetSizeOfAddressToByteMapArgHolde_impl(void*);
void* Create_IteratorForAddressToByteMapArgHolder_impl(void*);
void* IncrementIteratorForAddressToByteMapArgHolder_impl(void*, void*);
long long GetKeyForAddressToByteMapArgHolder_impl(void*);
signed int GetValueForAddressToUnsignedByteMapArgHolder_impl(void*);
void Delete_IteratorForAddressToByteMapArgHolder_impl(void*);

void* CreateControllerStateArgHolder_API_impl();

void SetControllerStateArgHolderValue_impl(void*, int, unsigned char);
int GetControllerStateArgHolderValue_impl(void*, int);

void* CreateListOfPointsArgHolder_API_impl();

unsigned int GetSizeOfListOfPointsArgHolder_impl(void*);
double GetListOfPointsXValueAtIndexForArgHolder_impl(void*, unsigned int);
double GetListOfPointsYValueAtIndexForArgHolder_impl(void*, unsigned int);
void ListOfPointsArgHolderPushBack_API_impl(void*, double, double);

void* CreateRegistrationInputTypeArgHolder_API_impl(void*);
void* CreateRegistrationWithAutoDeregistrationInputTypeArgHolder_API_impl(void*);
void* CreateRegistrationForButtonCallbackInputTypeArgHolder_API_impl(void*);
void* CreateUnregistrationInputTypeArgHolder_API_impl(void*);

void* GetVoidPointerFromArgHolder_impl(void*);

int GetBoolFromArgHolder_impl(void*);
unsigned long long GetU8FromArgHolder_impl(void*);
unsigned long long GetU16FromArgHolder_impl(void*);
unsigned long long GetU32FromArgHolder_impl(void*);
unsigned long long GetU64FromArgHolder_impl(void*);
signed long long GetS8FromArgHolder_impl(void*);
signed long long GetS16FromArgHolder_impl(void*);
signed long long GetS32FromArgHolder_impl(void*);
signed long long GetS64FromArgHolder_impl(void*);
double GetFloatFromArgHolder_impl(void*);
double* GetDoubleFromArgHolder_impl(void*);

// WARNING: The const char* returned from this method is only valid as long as the ArgHolder* is
// valid (i.e. until its delete method is called).
const char* GetStringFromArgHolder_impl(void*);

void Delete_ArgHolder_impl(
    void*);  // WARNING: Destroys the ArgHolder passed into it, and frees the associated memory.
             // You should either call this function on an ArgHolder, or call the
             // vector_of_arg_holder destructor once. You shouldn't call the vector destructor AND
             // the destructor for the ArgHolder - as this would cause a double delete!
#ifdef __cplusplus
}
#endif

}  // namespace Scripting

#endif

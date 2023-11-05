#pragma once

// Most files have their function APIs listed in the same file as their class is defined in.
// However, because ArgHolder has so many helper functions and so many APIs, I've put it into
// this separate file here (all the other API implementations are located in the file where their
// class definition is)

// These are the implementations for the APIs in ArgHolder_APIs
namespace Scripting
{

int GetArgType_API_impl(void*);
int GetIsEmpty_API_impl(void*);

void* CreateEmptyOptionalArgHolder_API_impl();

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
void* CreateBytesArgHolder_API_impl();
unsigned long long GetSizeOfBytesArgHolder_API_impl(void*);
void AddByteToBytesArgHolder_API_impl(void*, signed long long);
signed long long GetByteFromBytesArgHolderAtIndex_API_impl(void*, unsigned long long);
void* CreateGameCubeControllerStateArgHolder_API_impl();
void SetGameCubeControllerStateArgHolderValue_API_impl(void*, int, unsigned char);
int GetGameCubeControllerStateArgHolderValue_API_impl(void*, int);
void* CreateListOfPointsArgHolder_API_impl();
unsigned long long GetSizeOfListOfPointsArgHolder_API_impl(void*);
double GetListOfPointsXValueAtIndexForArgHolder_API_impl(void*, unsigned int);
double GetListOfPointsYValueAtIndexForArgHolder_API_impl(void*, unsigned int);
void ListOfPointsArgHolderPushBack_API_impl(void*, double, double);
void* CreateRegistrationInputTypeArgHolder_API_impl(void*);
void* CreateRegistrationWithAutoDeregistrationInputTypeArgHolder_API_impl(void*);
void* CreateRegistrationForButtonCallbackInputTypeArgHolder_API_impl(void*);
void* CreateUnregistrationInputTypeArgHolder_API_impl(void*);

void* GetVoidPointerFromArgHolder_API_impl(void*);
int GetBoolFromArgHolder_API_impl(void*);
unsigned long long GetU8FromArgHolder_API_impl(void*);
unsigned long long GetU16FromArgHolder_API_impl(void*);
unsigned long long GetU32FromArgHolder_API_impl(void*);
unsigned long long GetU64FromArgHolder_API_impl(void*);
signed long long GetS8FromArgHolder_API_impl(void*);
signed long long GetS16FromArgHolder_API_impl(void*);
signed long long GetS32FromArgHolder_API_impl(void*);
signed long long GetS64FromArgHolder_API_impl(void*);
double GetFloatFromArgHolder_API_impl(void*);
double GetDoubleFromArgHolder_API_impl(void*);
const char* GetStringFromArgHolder_API_impl(void*);
const char* GetErrorStringFromArgHolder_API_impl(void*);

void Delete_ArgHolder_API_impl(void*);

}  // namespace Scripting

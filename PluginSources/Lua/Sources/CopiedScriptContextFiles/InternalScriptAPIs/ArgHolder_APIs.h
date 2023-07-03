#ifndef ARG_HOLDER_APIS
#define ARG_HOLDER_APIS

#ifdef __cplusplus
extern "C" {
#endif

// IMPORTANT NOTE: All functions that start with "Create" allocate a new object on the heap. The DLL
// is reponsible for freeing them by calling Delete_ArgHolder on them when done using them.
typedef struct ArgHolder_APIs
{
  // Takes an opaque ArgHolder* as input, and returns its type as int (which represents an
  // ScriptingEnums::ArgTypeEnum)
  int (*GetArgType)(void*);

  // Returns 1 if the ArgHolder* passed into it was empty, and 0 otherwise.
  int (*GetIsEmpty)(void*);

  // Creates + returns an ArgHolder* which has no value inside of it.
  void* (*CreateEmptyOptionalArgHolder)();

  // Creates + returns a boolean ArgHolder* with the specified value.
  void* (*CreateBoolArgHolder)(int);

  // Creates + returns an unsigned_8 ArgHolder* with the specified value.
  void* (*CreateU8ArgHolder)(unsigned long long);

  // Creates + returns an unsigned_16 ArgHolder* with the specified value.
  void* (*CreateU16ArgHolder)(unsigned long long);

  // Creates + returns an unsigned_32 ArgHolder* with the specified value.
  void* (*CreateU32ArgHolder)(unsigned long long);

  // Creates + returns an unsigned_64 ArgHolder* with the specified value.
  void* (*CreateU64ArgHolder)(unsigned long long);

  // Creates + returns a signed_8 ArgHolder* with the specified value.
  void* (*CreateS8ArgHolder)(signed long long);

  // Creates + returns a signed_16 ArgHolder* with the specified value.
  void* (*CreateS16ArgHolder)(signed long long);

  // Creates + returns a signed_32 ArgHolder* with the specified value.
  void* (*CreateS32ArgHolder)(signed long long);

  // Creates + returns a signed_64 ArgHolder* with the specified value.
  void* (*CreateS64ArgHolder)(signed long long);

  // Creates + returns a float ArgHolder* with the specified value.
  void* (*CreateFloatArgHolder)(double);

  // Creates + returns a double ArgHolder* with the specified value.
  void* (*CreateDoubleArgHolder)(double);

  // Creates + returns a String ArgHolder* with the specified value.
  void* (*CreateStringArgHolder)(const char*);

  // Creates + returns a void* ArgHolder* with the specified value.
  void* (*CreateVoidPointerArgHolder)(void*);

  // This function creates + returns a Map<long long, s16> ArgHolder*. This is initially empty, but
  // the methods below can be used to get/set values in the returned ArgHolder*
  void* (*CreateAddressToByteMapArgHolder)();

  // Takes as its 1st parameter a Map<long long, s16> ArgHolder* (created by the function above), an
  // address as it's 2nd parameter, and the value of the address as its 3rd parameter. The function
  // adds the 2nd and 3rd parameters to the map contained in the 1st arg (the 2nd param is the key,
  // the 3rd param is the value)
  void (*AddPairToAddressToByteMapArgHolder)(void*, signed long long, signed long long);

  // Takes a Map<long long, s16> ArgHolder* as input, and returns the number of key-value pairs in
  // the map (i.e. the total number of keys in the map)
  unsigned long long (*GetSizeOfAddressToByteMapArgHolder)(void*);

  // WARNING: The following function allocates a new iterator on the heap, which must be deleted
  // using the Delete_IteratorForAddressToByteMapArgHolder() function to avoid a memory leak

  // Takes a Map<long long, s16> ArgHolder* as input, and returns an opaque handle to
  // an iterator*, which can be used to access the elements of the map in order (or
  // NULL, if the map is empty)
  void* (*CreateIteratorForAddressToByteMapArgHolder)(void*);

  // Takes an opaque handle to an iterator for a Map<long long, s16> ArgHolder* as its 1st param,
  // and an opaque handle to the ArgHolder* containing the Map<long long, s16> as the 2nd
  // param. The function calls the iterator's next method, and either returns an opaque handle to
  // the iterator, or NULL (if the end of the Map was reached).
  void* (*IncrementIteratorForAddressToByteMapArgHolder)(void*, void*);

  // Takes as input an opaque handle to an iterator for a Map<long long, s16> ArgHolder* as input,
  // and returns the key for the iterator
  signed long long (*GetKeyForAddressToByteMapArgHolder)(void*);

  // Takes as input an opaque handle to an iterator for a Map<long long, s16> ArgHolder* as input,
  // and returns the value for the iterator (of the key-value pair).
  signed long long (*GetValueForAddressToUnsignedByteMapArgHolder)(void*);

  // Takes as input an opaque handle to an iterator for a Map<long long, s16> ArgHolder*, and
  // deletes the iterator (frees the memory allocated to it).
  void (*Delete_IteratorForAddressToByteMapArgHolder)(void*);

  // Creates + returns an ArgHolder* to represent a GameCube Controller ControllerState. This is
  // initially set so that no buttons are pressed, joysticks are in the 128X128 default position, no
  // triggers are pressed, and the controller is connected. However, the functions below can be used
  // to modify the ControllerState struct contained in this ArgHolder*.
  void* (*CreateControllerStateArgHolder)();

  // Takes as its 1st param an ArgHolder* representing a GameCube ControllerState (returned by the
  // function above). The 2nd parameter represents the GCButtonNameEnum that represents the name of
  // the button, and the 3rd parameter either represents a boolean for a digital button (with 1 as
  // true and 0 as false), or a value between 0-255 for an analog trigger/joystick. The meaning of
  // the 3rd param depends on whether the 2nd param refers to an analog or digital button. The
  // function updates the ControllerState in the ArgHolder accordingly.
  void (*SetControllerStateArgHolderValue)(void*, int, unsigned char);

  // Takes as its 1st param an ArgHolder* representing a GameCube ControllerState. The 2nd parameter
  // represents the GCButtonNameEnum that represents the name of the button. If the button is an
  // analog type, then the function returns a value between 0-255. Otherwise, the function will
  // return either 1 or 0 (to indicate pressed vs. not pressed for digital buttons).
  int (*GetControllerStateArgHolderValue)(void*, int);

  // Creates + returns an ArgHolder* which represents a list of points (each point is represented by
  // 2 float values, which represent X and Y values). This list is initally empty, but can be set
  // using the functions below.
  void* (*CreateListOfPointsArgHolder)();

  // Returns the size of the list of points from the passed in ArgHolder* that represents a list of
  // points (the return value of the function above).
  unsigned long long (*GetSizeOfListOfPointsArgHolder)(void*);

  // Takes as input an ArgHolder* of a list of points, finds the point at the specified index in the
  // list (the 2nd param), and returns the X value of the point (the 1st value in the tuple of 2
  // floats).
  double (*GetListOfPointsXValueAtIndexForArgHolder)(void*, unsigned int);

  // Takes as input an ArgHolder* of a list of points, finds the point at the
  // specified index in the list (the 2nd parameter), and returns the Y
  // value of the point (the 2nd value in the tuple of 2 floats).
  double (*GetListOfPointsYValueAtIndexForArgHolder)(void*, unsigned int);

  // Takes as input an ArgHolder* representing a list of points (1st param), a double representing
  // the X value of the point (the 2nd param), and a double representing the Y value of the point
  // (the 3rd param). The function adds the specified point to the list of points in the ArgHolder*
  void (*ListOfPointsArgHolderPushBack)(void*, double, double);

  // Creates + returns an ArgHolder* whose void* contains a RegistrationInputType (defined by the
  // DLL)
  void* (*CreateRegistrationInputTypeArgHolder)(void*);

  // Creates + returns an ArgHolder* whose void* contains an auto-deregistration input type (defined
  // by the DLL)
  void* (*CreateRegistrationWithAutoDeregistrationInputTypeArgHolder)(void*);

  // Creates + returns an ArgHolder* whose void* contains a RegistrationForButtonCallback input type
  // (defined by the DLL)
  void* (*CreateRegistrationForButtonCallbackInputTypeArgHolder)(void*);

  // Creates + returns an ArgHolder* whose void* contains an Unregistration input type
  // (defined by the DLL).
  void* (*CreateUnregistrationInputTypeArgHolder)(void*);

  // Takes as input an ArgHolder* which stores its data in a void*. Returns the void* stored inside
  // of the ArgHolder*
  void* (*GetVoidPointerFromArgHolder)(void*);

  int (*GetBoolFromArgHolder)(void*);               // Gets and returns the bool from the ArgHolder*
  unsigned long long (*GetU8FromArgHolder)(void*);  // Gets and returns the u8 from the ArgHolder*
  unsigned long long (*GetU16FromArgHolder)(void*);  // Gets and returns the u16 from the ArgHolder*
  unsigned long long (*GetU32FromArgHolder)(void*);  // Gets and returns the u32 from the ArgHolder*
  unsigned long long (*GetU64FromArgHolder)(void*);  // Gets and returns the u64 from the ArgHolder*
  signed long long (*GetS8FromArgHolder)(void*);     // Gets and returns the s8 from the ArgHolder*
  signed long long (*GetS16FromArgHolder)(void*);    // Gets and returns the s16 from the ArgHolder*
  signed long long (*GetS32FromArgHolder)(void*);    // Gets and returns the s32 from the ArgHolder*
  signed long long (*GetS64FromArgHolder)(void*);    // Gets and returns the s64 from the ArgHolder*
  double (*GetFloatFromArgHolder)(void*);   // Gets and returns the float from the ArgHolder*
  double (*GetDoubleFromArgHolder)(void*);  // Gets and returns the double from the ArgHolder*

  // WARNING: The const char* returned from these 2 methods is only valid as long as the ArgHolder*
  // is valid (i.e. until its delete method is called).

  // Gets and returns the String from the ArgHolder* as a const char*.
  const char* (*GetStringFromArgHolder)(void*);

  // Gets and returns the Error String from the ArgHolder* as a const char*
  const char* (*GetErrorStringFromArgHolder)(void*);

  // WARNING: This funcntion destroys the ArgHolder* passed into it, and frees the associated
  // memory. You should either call this function on an ArgHolder*, or call the vector_of_arg_holder
  // destructor once. You shouldn't call the vector destructor AND the destructor for the ArgHolder*
  // (assuming the ArgHolder* was also added to the vector) - as this would cause a double delete!
  void (*Delete_ArgHolder)(void*);

} ArgHolder_APIs;

#ifdef __cplusplus
}
#endif

#endif

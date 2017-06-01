////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2013 Jonathan De Wachter (dewachter.jonathan@gmail.com)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/Android/InputImpl.hpp>
#include <SFML/System/Android/Activity.hpp>
#include <SFML/System/Lock.hpp>
#include <SFML/System/Err.hpp>
#include <jni.h>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
bool InputImpl::isKeyPressed(Keyboard::Key key)
{
    // Not applicable
    return false;
}

////////////////////////////////////////////////////////////
void InputImpl::setVirtualKeyboardVisible(bool visible)
{
    // todo: Check if the window is active

    ActivityStates* states = getActivity(NULL);
    Lock lock(states->mutex);

    // Initializes JNI
    jint lResult;
    jint lFlags = 0;

    JavaVM* lJavaVM = states->activity->vm;
    JNIEnv* lJNIEnv = states->activity->env;

    JavaVMAttachArgs lJavaVMAttachArgs;
    lJavaVMAttachArgs.version = JNI_VERSION_1_6;
    lJavaVMAttachArgs.name = "NativeThread";
    lJavaVMAttachArgs.group = NULL;

    lResult=lJavaVM->AttachCurrentThread(&lJNIEnv, &lJavaVMAttachArgs);

    if (lResult == JNI_ERR)
        err() << "Failed to initialize JNI, couldn't switch the keyboard visibility" << std::endl;

    // Retrieves NativeActivity
    jobject lNativeActivity = states->activity->clazz;
    jclass ClassNativeActivity = lJNIEnv->GetObjectClass(lNativeActivity);

    // Retrieves Context.INPUT_METHOD_SERVICE
    jclass ClassContext = lJNIEnv->FindClass("android/content/Context");
    jfieldID FieldINPUT_METHOD_SERVICE = lJNIEnv->GetStaticFieldID(ClassContext,
        "INPUT_METHOD_SERVICE", "Ljava/lang/String;");
    jobject INPUT_METHOD_SERVICE = lJNIEnv->GetStaticObjectField(ClassContext,
        FieldINPUT_METHOD_SERVICE);
    lJNIEnv->DeleteLocalRef(ClassContext);

    // Runs getSystemService(Context.INPUT_METHOD_SERVICE)
    jclass ClassInputMethodManager =
        lJNIEnv->FindClass("android/view/inputmethod/InputMethodManager");
    jmethodID MethodGetSystemService = lJNIEnv->GetMethodID(ClassNativeActivity,
        "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jobject lInputMethodManager = lJNIEnv->CallObjectMethod(lNativeActivity,
        MethodGetSystemService, INPUT_METHOD_SERVICE);
    lJNIEnv->DeleteLocalRef(INPUT_METHOD_SERVICE);

    // Runs getWindow().getDecorView()
    jmethodID MethodGetWindow = lJNIEnv->GetMethodID(ClassNativeActivity,
        "getWindow", "()Landroid/view/Window;");
    jobject lWindow = lJNIEnv->CallObjectMethod(lNativeActivity, MethodGetWindow);
    jclass ClassWindow = lJNIEnv->FindClass("android/view/Window");
    jmethodID MethodGetDecorView = lJNIEnv->GetMethodID(ClassWindow,
        "getDecorView", "()Landroid/view/View;");
    jobject lDecorView = lJNIEnv->CallObjectMethod(lWindow, MethodGetDecorView);
    lJNIEnv->DeleteLocalRef(lWindow);
    lJNIEnv->DeleteLocalRef(ClassWindow);

    if (visible)
    {
        // Runs lInputMethodManager.showSoftInput(...)
        jmethodID MethodShowSoftInput = lJNIEnv->GetMethodID(ClassInputMethodManager,
            "showSoftInput", "(Landroid/view/View;I)Z");
        jboolean lResult = lJNIEnv->CallBooleanMethod(lInputMethodManager,
            MethodShowSoftInput, lDecorView, lFlags);
    }
    else
    {
        // Runs lWindow.getViewToken()
        jclass ClassView = lJNIEnv->FindClass("android/view/View");
        jmethodID MethodGetWindowToken = lJNIEnv->GetMethodID(ClassView,
            "getWindowToken", "()Landroid/os/IBinder;");
        jobject lBinder = lJNIEnv->CallObjectMethod(lDecorView,
            MethodGetWindowToken);
        lJNIEnv->DeleteLocalRef(ClassView);

        // lInputMethodManager.hideSoftInput(...)
        jmethodID MethodHideSoftInput = lJNIEnv->GetMethodID(ClassInputMethodManager,
            "hideSoftInputFromWindow", "(Landroid/os/IBinder;I)Z");
        jboolean lRes = lJNIEnv->CallBooleanMethod(lInputMethodManager,
            MethodHideSoftInput, lBinder, lFlags);
        lJNIEnv->DeleteLocalRef(lBinder);
    }
    lJNIEnv->DeleteLocalRef(ClassNativeActivity);
    lJNIEnv->DeleteLocalRef(ClassInputMethodManager);
    lJNIEnv->DeleteLocalRef(lDecorView);

    // Finished with the JVM
    lJavaVM->DetachCurrentThread();
}

////////////////////////////////////////////////////////////
bool InputImpl::isMouseButtonPressed(Mouse::Button button)
{
    ALooper_pollAll(0, NULL, NULL, NULL);

    priv::ActivityStates* states = priv::getActivity(NULL);
    Lock lock(states->mutex);

    return states->isButtonPressed[button];
}


////////////////////////////////////////////////////////////
Vector2i InputImpl::getMousePosition()
{
    ALooper_pollAll(0, NULL, NULL, NULL);

    priv::ActivityStates* states = priv::getActivity(NULL);
    Lock lock(states->mutex);

    return states->mousePosition;
}


////////////////////////////////////////////////////////////
Vector2i InputImpl::getMousePosition(const Window& relativeTo)
{
    return getMousePosition();
}


////////////////////////////////////////////////////////////
void InputImpl::setMousePosition(const Vector2i& position)
{
    // Injecting events is impossible on Android
}


////////////////////////////////////////////////////////////
void InputImpl::setMousePosition(const Vector2i& position, const Window& relativeTo)
{
    setMousePosition(position);
}


////////////////////////////////////////////////////////////
bool InputImpl::isTouchDown(unsigned int finger)
{
    ALooper_pollAll(0, NULL, NULL, NULL);

    priv::ActivityStates* states = priv::getActivity(NULL);
    Lock lock(states->mutex);

    return states->touchEvents.find(finger) != states->touchEvents.end();
}


////////////////////////////////////////////////////////////
Vector2i InputImpl::getTouchPosition(unsigned int finger)
{
    ALooper_pollAll(0, NULL, NULL, NULL);

    priv::ActivityStates* states = priv::getActivity(NULL);
    Lock lock(states->mutex);

    return states->touchEvents.find(finger)->second;
}


////////////////////////////////////////////////////////////
Vector2i InputImpl::getTouchPosition(unsigned int finger, const Window& relativeTo)
{
    return getTouchPosition(finger);
}

} // namespace priv

} // namespace sf

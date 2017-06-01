////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Laurent Gomila (laurent@sfml-dev.org)
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
// Android specific: we define the ANativeActivity_onCreate
// entry point, handling all the native activity stuff, then
// we call the user defined (and portable) main function in
// an external thread so developers can keep a portable code
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Config.hpp>

#ifdef SFML_SYSTEM_ANDROID

#include <SFML/System/Android/Activity.hpp>
#include <SFML/System/Sleep.hpp>
#include <SFML/System/Thread.hpp>
#include <SFML/System/Lock.hpp>
#include <SFML/System/Err.hpp>
#include <android/window.h>
#include <android/native_activity.h>


extern int main(int argc, char *argv[]);

namespace sf
{
namespace priv
{

////////////////////////////////////////////////////////////
int getAndroidApiLevel(ANativeActivity* activity)
{
    JNIEnv* lJNIEnv = activity->env;

    jclass versionClass = lJNIEnv->FindClass("android/os/Build$VERSION");
    if (versionClass == NULL)
        return 0;

    jfieldID sdkIntFieldID = lJNIEnv->GetStaticFieldID(versionClass, "SDK_INT", "I");
    if (sdkIntFieldID == NULL)
        return 0;
    
    jint sdkInt = 0;
    sdkInt = lJNIEnv->GetStaticIntField(versionClass, sdkIntFieldID);
    
    return sdkInt;
}


////////////////////////////////////////////////////////////
ActivityStates* retrieveStates(ANativeActivity* activity)
{
    // Hide the ugly cast we find in each callback function
    return (ActivityStates*)activity->instance;
}


////////////////////////////////////////////////////////////
static void initializeMain(ActivityStates* states)
{
    // Protect from concurrent access
    Lock lock(states->mutex);

    // Prepare and share the looper to be read later
    ALooper* looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    states->looper = looper;

    // Get the default configuration
    states->config = AConfiguration_new();
    AConfiguration_fromAssetManager(states->config, states->activity->assetManager);
}


////////////////////////////////////////////////////////////
static void terminateMain(ActivityStates* states)
{
    // Protect from concurrent access
    Lock lock(states->mutex);

    // The main thread has finished, we must explicitly ask the activity to finish
    states->mainOver = true;
    ANativeActivity_finish(states->activity);
}


////////////////////////////////////////////////////////////
void* main(ActivityStates* states)
{
    // Initialize the thread before giving the hand
    initializeMain(states);

    sleep(seconds(0.5));
    ::main(0, NULL);

    // Terminate properly the main thread and wait until it's done
    terminateMain(states);

    {
        Lock lock(states->mutex);

        states->terminated = true;
    }

    return NULL;
}

} // namespace priv
} // namespace sf


////////////////////////////////////////////////////////////
void goToFullscreenMode(ANativeActivity* activity)
{
    // Get the current Android API level.
    int apiLevel = sf::priv::getAndroidApiLevel(activity);

    // Hide the status bar
    ANativeActivity_setWindowFlags(activity, AWINDOW_FLAG_FULLSCREEN,
        AWINDOW_FLAG_FULLSCREEN);

    // Hide the navigation bar
    JavaVM* lJavaVM = activity->vm;
    JNIEnv* lJNIEnv = activity->env;

    jobject objectActivity = activity->clazz;
    jclass classActivity = lJNIEnv->GetObjectClass(objectActivity);

    jmethodID methodGetWindow = lJNIEnv->GetMethodID(classActivity, "getWindow", "()Landroid/view/Window;");
    jobject objectWindow = lJNIEnv->CallObjectMethod(objectActivity, methodGetWindow);

    jclass classWindow = lJNIEnv->FindClass("android/view/Window");
    jmethodID methodGetDecorView = lJNIEnv->GetMethodID(classWindow, "getDecorView", "()Landroid/view/View;");
    jobject objectDecorView = lJNIEnv->CallObjectMethod(objectWindow, methodGetDecorView);

    jclass classView = lJNIEnv->FindClass("android/view/View");

    // Default flags
    jint flags = 0;
    
    // API Level 14
    if (apiLevel >= 14)
    {
        jfieldID FieldSYSTEM_UI_FLAG_LOW_PROFILE = lJNIEnv->GetStaticFieldID(classView, "SYSTEM_UI_FLAG_LOW_PROFILE", "I");
        jint SYSTEM_UI_FLAG_LOW_PROFILE = lJNIEnv->GetStaticIntField(classView, FieldSYSTEM_UI_FLAG_LOW_PROFILE);
        flags |= SYSTEM_UI_FLAG_LOW_PROFILE;
    }

    // API Level 16
    if (apiLevel >= 16)
    {
        jfieldID FieldSYSTEM_UI_FLAG_FULLSCREEN = lJNIEnv->GetStaticFieldID(classView, "SYSTEM_UI_FLAG_FULLSCREEN", "I");
        jint SYSTEM_UI_FLAG_FULLSCREEN = lJNIEnv->GetStaticIntField(classView, FieldSYSTEM_UI_FLAG_FULLSCREEN);
        flags |= SYSTEM_UI_FLAG_FULLSCREEN;
    }

    // API Level 19
    if (apiLevel >= 19)
    {
        jfieldID FieldSYSTEM_UI_FLAG_IMMERSIVE_STICKY  = lJNIEnv->GetStaticFieldID(classView, "SYSTEM_UI_FLAG_IMMERSIVE_STICKY", "I");
        jint SYSTEM_UI_FLAG_IMMERSIVE_STICKY = lJNIEnv->GetStaticIntField(classView, FieldSYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        flags |= SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
    }

    jmethodID methodsetSystemUiVisibility = lJNIEnv->GetMethodID(classView, "setSystemUiVisibility", "(I)V");
    lJNIEnv->CallVoidMethod(objectDecorView, methodsetSystemUiVisibility, flags);
}

////////////////////////////////////////////////////////////

void getScreenSizeInPixels(ANativeActivity* activity, int* width, int* height)
{
    // Perform the following Java code:
    //
    // DisplayMetrics dm = new DisplayMetrics();
    // getWindowManager().getDefaultDisplay().getMetrics(dm);

    JavaVM* lJavaVM = activity->vm;
    JNIEnv* lJNIEnv = activity->env;

    jobject objectActivity = activity->clazz;
    jclass classActivity = lJNIEnv->GetObjectClass(objectActivity);

    jclass classDisplayMetrics = lJNIEnv->FindClass("android/util/DisplayMetrics");
    jmethodID initDisplayMetrics = lJNIEnv->GetMethodID(classDisplayMetrics, "<init>", "()V");
    jobject objectDisplayMetrics = lJNIEnv->NewObject(classDisplayMetrics, initDisplayMetrics);

    jmethodID methodGetWindowManager = lJNIEnv->GetMethodID(classActivity, "getWindowManager", "()Landroid/view/WindowManager;");
    jobject objectWindowManager = lJNIEnv->CallObjectMethod(objectActivity, methodGetWindowManager);

    jclass classWindowManager = lJNIEnv->FindClass("android/view/WindowManager");
    jmethodID methodGetDefaultDisplay = lJNIEnv->GetMethodID(classWindowManager, "getDefaultDisplay", "()Landroid/view/Display;");
    jobject objectDisplay = lJNIEnv->CallObjectMethod(objectWindowManager, methodGetDefaultDisplay);

    jclass classDisplay = lJNIEnv->FindClass("android/view/Display");
    jmethodID methodGetMetrics = lJNIEnv->GetMethodID(classDisplay, "getMetrics", "(Landroid/util/DisplayMetrics;)V");
    lJNIEnv->CallVoidMethod(objectDisplay, methodGetMetrics, objectDisplayMetrics);

    jfieldID fieldWidthPixels = lJNIEnv->GetFieldID(classDisplayMetrics, "widthPixels", "I");
    jfieldID fieldHeightPixels = lJNIEnv->GetFieldID(classDisplayMetrics, "heightPixels", "I");

    *width = lJNIEnv->GetIntField(objectDisplayMetrics, fieldWidthPixels);
    *height = lJNIEnv->GetIntField(objectDisplayMetrics, fieldHeightPixels);
}


////////////////////////////////////////////////////////////
static void onStart(ANativeActivity* activity)
{
}


////////////////////////////////////////////////////////////
static void onResume(ANativeActivity* activity)
{
    // Retrieve our activity states from the activity instance
    sf::priv::ActivityStates* states = sf::priv::retrieveStates(activity);
    sf::Lock lock(states->mutex);

    if (states->fullscreen)
        goToFullscreenMode(activity);

    // Send an event to warn people the activity has been resumed
    sf::Event event;
    event.type = sf::Event::MouseEntered;

    states->forwardEvent(event);
}


////////////////////////////////////////////////////////////
static void onPause(ANativeActivity* activity)
{
    // Retrieve our activity states from the activity instance
    sf::priv::ActivityStates* states = sf::priv::retrieveStates(activity);
    sf::Lock lock(states->mutex);

    // Send an event to warn people the activity has been paused
    sf::Event event;
    event.type = sf::Event::MouseLeft;

    states->forwardEvent(event);
}


////////////////////////////////////////////////////////////
static void onStop(ANativeActivity* activity)
{
}


////////////////////////////////////////////////////////////
static void onDestroy(ANativeActivity* activity)
{
    // Retrieve our activity states from the activity instance
    sf::priv::ActivityStates* states = sf::priv::retrieveStates(activity);

    // Send an event to warn people the activity is being destroyed
    {
        sf::Lock lock(states->mutex);

        // If the main thread hasn't yet finished, send the event and wait for
        // it to finish.
        if (!states->mainOver)
        {
            sf::Event event;
            event.type = sf::Event::Closed;

            states->forwardEvent(event);
        }
    }

    // Wait for the main thread to be terminated
    states->mutex.lock();

    while (!states->terminated)
    {
        states->mutex.unlock();
        sf::sleep(sf::milliseconds(20));
        states->mutex.lock();
    }

    states->mutex.unlock();

    // Terminate EGL display
    eglTerminate(states->display);

    // Delete our allocated states
    delete states;

    // Reset the activity pointer for all modules
    sf::priv::getActivity(NULL, true);

    // The application should now terminate
}


////////////////////////////////////////////////////////////
static void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window)
{
    sf::priv::ActivityStates* states = sf::priv::retrieveStates(activity);
    sf::Lock lock(states->mutex);

    // Update the activity states
    states->window = window;

    // Notify SFML mechanism
    sf::Event event;
    event.type = sf::Event::GainedFocus;
    states->forwardEvent(event);

    // Wait for the event to be taken into account by SFML
    states->updated = false;
    while(!states->updated)
    {
        states->mutex.unlock();
        sf::sleep(sf::milliseconds(10));
        states->mutex.lock();
    }
}


////////////////////////////////////////////////////////////
static void onNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window)
{
    sf::priv::ActivityStates* states = sf::priv::retrieveStates(activity);
    sf::Lock lock(states->mutex);

    // Update the activity states
    states->window = NULL;

    // Notify SFML mechanism
    sf::Event event;
    event.type = sf::Event::LostFocus;
    states->forwardEvent(event);

    // Wait for the event to be taken into account by SFML
    states->updated = false;
    while(!states->updated)
    {
        states->mutex.unlock();
        sf::sleep(sf::milliseconds(10));
        states->mutex.lock();
    }
}


////////////////////////////////////////////////////////////
static void onNativeWindowRedrawNeeded(ANativeActivity* activity, ANativeWindow* window)
{
}


////////////////////////////////////////////////////////////
static void onNativeWindowResized(ANativeActivity* activity, ANativeWindow* window)
{
}


////////////////////////////////////////////////////////////
static void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue)
{
    // Retrieve our activity states from the activity instance
    sf::priv::ActivityStates* states = sf::priv::retrieveStates(activity);

    // Attach the input queue
    {
        sf::Lock lock(states->mutex);

        AInputQueue_attachLooper(queue, states->looper, 1, states->processEvent, NULL);
        states->inputQueue = queue;
    }
}


////////////////////////////////////////////////////////////
static void onInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue)
{
    // Retrieve our activity states from the activity instance
    sf::priv::ActivityStates* states = sf::priv::retrieveStates(activity);

    // Detach the input queue
    {
        sf::Lock lock(states->mutex);

        states->inputQueue = NULL;
        AInputQueue_detachLooper(queue);
    }
}


////////////////////////////////////////////////////////////
static void onWindowFocusChanged(ANativeActivity* activity, int focused)
{
}


////////////////////////////////////////////////////////////
static void onContentRectChanged(ANativeActivity* activity, const ARect* rect)
{
    // Retrieve our activity states from the activity instance
    sf::priv::ActivityStates* states = sf::priv::retrieveStates(activity);
    sf::Lock lock(states->mutex);

    // Make sure the window still exists before we access the dimensions on it
    if (states->window != NULL) {
        // Send an event to warn people about the window move/resize
        sf::Event event;
        event.type = sf::Event::Resized;
        event.size.width = ANativeWindow_getWidth(states->window);
        event.size.height = ANativeWindow_getHeight(states->window);

        states->forwardEvent(event);
    }
}


////////////////////////////////////////////////////////////
static void onConfigurationChanged(ANativeActivity* activity)
{
}


////////////////////////////////////////////////////////////
static void* onSaveInstanceState(ANativeActivity* activity, size_t* outLen)
{
    *outLen = 0;

    return NULL;
}


////////////////////////////////////////////////////////////
static void onLowMemory(ANativeActivity* activity)
{
}


////////////////////////////////////////////////////////////
void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize)
{
    // Create an activity states (will keep us in the know, about events we care)
    sf::priv::ActivityStates* states = NULL;
    states = new sf::priv::ActivityStates;

    // Initialize the states value
    states->activity   = NULL;
    states->window     = NULL;
    states->looper     = NULL;
    states->inputQueue = NULL;
    states->config     = NULL;

    for (unsigned int i = 0; i < sf::Mouse::ButtonCount; i++)
        states->isButtonPressed[i] = false;

    states->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if (savedState != NULL)
    {
        states->savedState = malloc(savedStateSize);
        states->savedStateSize = savedStateSize;
        memcpy(states->savedState, savedState, savedStateSize);
    }

    states->mainOver = false;

    states->initialized = false;
    states->terminated = false;

    // Share it across the SFML modules
    sf::priv::getActivity(states, true);

    // These functions will update the activity states and therefore, will allow
    // SFML to be kept in the know
    activity->callbacks->onStart   = onStart;
    activity->callbacks->onResume  = onResume;
    activity->callbacks->onPause   = onPause;
    activity->callbacks->onStop    = onStop;
    activity->callbacks->onDestroy = onDestroy;

    activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
    activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
    activity->callbacks->onNativeWindowRedrawNeeded = onNativeWindowRedrawNeeded;
    activity->callbacks->onNativeWindowResized = onNativeWindowResized;

    activity->callbacks->onInputQueueCreated = onInputQueueCreated;
    activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;

    activity->callbacks->onWindowFocusChanged = onWindowFocusChanged;
    activity->callbacks->onContentRectChanged = onContentRectChanged;
    activity->callbacks->onConfigurationChanged = onConfigurationChanged;

    activity->callbacks->onSaveInstanceState = onSaveInstanceState;
    activity->callbacks->onLowMemory = onLowMemory;

    // Share this activity with the callback functions
    states->activity = activity;

    // Keep the screen turned on and bright
    ANativeActivity_setWindowFlags(activity, AWINDOW_FLAG_KEEP_SCREEN_ON,
        AWINDOW_FLAG_KEEP_SCREEN_ON);

    // Initialize the display
    eglInitialize(states->display, NULL, NULL);

    getScreenSizeInPixels(activity, &states->screenSize.x, &states->screenSize.y);

    // Redirect error messages to logcat
    sf::err().rdbuf(&states->logcat);

    // Launch the main thread
    sf::Thread* thread = new sf::Thread(sf::priv::main, states);
    thread->launch();

    // Wait for the main thread to be initialized
    states->mutex.lock();

    while (!states->initialized)
    {
        states->mutex.unlock();
        sf::sleep(sf::milliseconds(20));
        states->mutex.lock();
    }

    states->mutex.unlock();

    // Share this state with the callback functions
    activity->instance = states;
}

#endif // SFML_SYSTEM_ANDROID

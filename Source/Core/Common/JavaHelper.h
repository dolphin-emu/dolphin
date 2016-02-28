// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <jni.h>

namespace JavaHelper
{
	void Init(jobject class_loader);
	void Shutdown();

	// Called from JNI_OnLoad
	void SetVM(JavaVM* vm);
	// Get the global JavaVM object
	JavaVM* GetVM();

	// Find a class using the application's class loader
	jclass FindClass(const std::string& class_sig);

	// Returns true if we attached in this call
	// False if this thread was attached prior
	bool AttachThread(JNIEnv** env);
	void DetachThread(bool skip = false);
}

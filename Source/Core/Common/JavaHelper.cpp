// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <jni.h>

#include "Common/JavaHelper.h"

namespace JavaHelper
{
	static JavaVM* s_vm;

	static jobject s_loader_object;
	static jmethodID s_loader_load;

	static void LoadClassLoader(jobject class_loader)
	{
		JNIEnv* env;
		bool attached = AttachThread(&env);

		s_loader_object = env->NewGlobalRef(class_loader);

		jclass loader_class = env->FindClass("java/lang/ClassLoader");
		s_loader_load = env->GetMethodID(loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

		DetachThread(!attached);
	}

	void Init(jobject class_loader)
	{
		LoadClassLoader(class_loader);
	}

	void Shutdown()
	{
		JNIEnv* env;
		bool attached = AttachThread(&env);

		env->DeleteGlobalRef(s_loader_object);

		DetachThread(!attached);
	}

	void SetVM(JavaVM* vm)
	{
		s_vm = vm;
	}

	JavaVM* GetVM()
	{
		return s_vm;
	}

	jclass FindClass(const std::string& class_sig)
	{
		JNIEnv* env;
		bool attached = AttachThread(&env);

		jstring jclass_sig = env->NewStringUTF(class_sig.c_str());

		jclass res = reinterpret_cast<jclass>(env->CallObjectMethod(s_loader_object, s_loader_load, jclass_sig));

		env->DeleteLocalRef(jclass_sig);

		jthrowable exc = env->ExceptionOccurred();

		if (exc)
			res = nullptr; // For now we'll let Java handle the exception

		DetachThread(!attached);
		return res;
	}

	bool AttachThread(JNIEnv** env)
	{
		int get_env_status = s_vm->GetEnv(reinterpret_cast<void**>(env), JNI_VERSION_1_6);

		if (get_env_status == JNI_EDETACHED)
			s_vm->AttachCurrentThread(env, nullptr);

		return get_env_status == JNI_EDETACHED;
	}

	void DetachThread(bool skip)
	{
		if (!skip)
			s_vm->DetachCurrentThread();
	}
}

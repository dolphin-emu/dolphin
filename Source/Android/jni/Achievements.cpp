// Copyright 2003 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <jni.h>
#include <map>
#include <thread>

#include "Achievements.h"

#include "Common/Event.h"
#include "Common/Thread.h"
#include "Common/Logging/Log.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "DiscIO/CompressedBlob.h"

// Global java_vm class
extern JavaVM* g_java_vm;

namespace Achievements
{
// Achievement handling function
static bool InTheCloset(AchievementTypes type)
{
	if (Core::GetState() == Core::CORE_UNINITIALIZED || Core::GetState() == Core::CORE_PAUSE)
		return false;

	if (DiscIO::IsGCZBlob(SConfig::GetInstance().m_strFilename))
		Achievements::UnlockAchievement(type);
	return true;
}

// Java objects
JNIEnv* s_env;
jclass s_achieve_class;
jmethodID s_unlock_func;
jmethodID s_increment_func;

std::thread s_handle_thread;
Common::Flag s_thread_running;
std::map<AchievementTypes, std::pair<std::function<bool(AchievementTypes)>, bool>> s_achieve_map =
{
	{ AchievementTypes::ACHIEVEMENT_CLOSETSPACE, {InTheCloset, false}},
};
const std::map<AchievementTypes, std::string> s_achieve_ids =
{
	{ AchievementTypes::ACHIEVEMENT_CLOSETSPACE, "CgkIzbmMkdIfEAIQCQ" },
};

static void HandleThread()
{
	Common::SetCurrentThreadName("Android Achievements Thread");
	NOTICE_LOG(SERIALINTERFACE, "Android achievements thread started");

	g_java_vm->AttachCurrentThread(&s_env, NULL);

	// Initialize our java bits
	s_unlock_func = s_env->GetStaticMethodID(s_achieve_class, "UnlockAchievement", "(Ljava/lang/String;)V");
	s_increment_func = s_env->GetStaticMethodID(s_achieve_class, "IncrementAchievement", "(Ljava/lang/String;I)V");

	while (s_thread_running.IsSet())
	{
		for (auto& handlers : s_achieve_map)
			if (!handlers.second.second)
				handlers.second.second = handlers.second.first(handlers.first);

		Common::SleepCurrentThread(5000);
	}

	g_java_vm->DetachCurrentThread();
	NOTICE_LOG(SERIALINTERFACE, "Android achievements thread ended");
}


void UnlockAchievement(AchievementTypes type)
{
	auto id = s_achieve_ids.find(type);
	if (id == s_achieve_ids.end())
		return;

	jstring jstr = s_env->NewStringUTF(id->second.c_str());
	s_env->CallStaticVoidMethod(s_achieve_class, s_unlock_func, jstr);
}

void IncrementAchievement(AchievementTypes type, int amount)
{
	auto id = s_achieve_ids.find(type);
	if (id == s_achieve_ids.end())
		return;

	jstring jstr = s_env->NewStringUTF(id->second.c_str());
	s_env->CallStaticVoidMethod(s_achieve_class, s_increment_func, jstr, amount);
}

void Init()
{
	JNIEnv* env;
	g_java_vm->AttachCurrentThread(&env, NULL);

	jclass achieve_class = env->FindClass("org/dolphinemu/dolphinemu/utils/AchievementHandler");
	s_achieve_class = reinterpret_cast<jclass>(env->NewGlobalRef(achieve_class));

	s_thread_running.Set(true);
	s_handle_thread = std::thread(HandleThread);
}
void Shutdown()
{
	if (s_thread_running.TestAndClear())
		s_handle_thread.join();
}

}

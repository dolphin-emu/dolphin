// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "Common/Event.h"
#include "Common/FifoQueue.h"
#include "Common/Thread.h"

#include "Core/PowerPC/TieredRecompiler.h"
#if HAS_LLVM
#include "Core/PowerPC/JitLLVM/Jit.h"
#endif

namespace TieredRecompiler
{
	bool m_running = false;
	std::thread m_rec_thread;
	Common::Event m_has_work;
	Common::FifoQueue<u32, true> m_work_queue;
#if HAS_LLVM
	std::unique_ptr<JitLLVM> m_llvm_jit;
#endif

	static void RecompilerThread()
	{
		Common::SetCurrentThreadName("Tiered Recompiler");
		m_running = true;

		while (m_running)
		{
			if (!m_work_queue.Size())
				m_has_work.Wait();

			u32 work_address = 0;
			if (m_work_queue.Pop(work_address))
			{
				// Looks like we have a bit of work
#if HAS_LLVM
				m_llvm_jit->Jit(work_address);
#endif
			}
		}
	}

	bool Init()
	{
#if HAS_LLVM
		m_llvm_jit.reset(new JitLLVM());
		m_llvm_jit->Init();
		m_rec_thread = std::thread(RecompilerThread);
#endif
		return true;
	}

	void Shutdown()
	{
		m_running = false;
		m_has_work.Set(); // Give the event in the thread a kick
		m_rec_thread.join();
		m_work_queue.Clear();
#if HAS_LLVM
		delete m_llvm_jit.release();
#endif
	}

	void AddWorkToQueue(u32 em_address)
	{
#if HAS_LLVM
		m_work_queue.Push(em_address);
		m_has_work.Set();
#endif
	}
}

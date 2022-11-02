// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Adapted from the yuzu Emulator Project
// which in turn is adapted from DXVK

#include "VKScheduler.h"
#include <Common/Assert.h>
#include "Common/Thread.h"
#include "StateTracker.h"

#include "VideoBackends/Vulkan/VKTimelineSemaphore.h"

namespace Vulkan
{
Scheduler::Scheduler(bool use_worker_thread)
    : m_use_worker_thread(use_worker_thread), m_semaphore(std::make_unique<VKTimelineSemaphore>()),
      m_command_buffer_manager(std::make_unique<CommandBufferManager>(m_semaphore.get(), true))
{
  AcquireNewChunk();

  if (m_use_worker_thread)
  {
    m_submit_loop = std::make_unique<Common::BlockingLoop>();
    m_worker = std::thread([this]() {
      Common::SetCurrentThreadName("Vulkan CS Thread");
      WorkerThread();
    });
  }
}

Scheduler::~Scheduler()
{
  if (m_use_worker_thread)
  {
    SyncWorker();
    m_submit_loop->Stop();
    m_worker.join();
  }

  m_semaphore.reset();
  m_command_buffer_manager.reset();
}

bool Scheduler::Initialize()
{
  return m_command_buffer_manager->Initialize();
}

void Scheduler::CommandChunk::ExecuteAll(CommandBufferManager* cmdbuf)
{
  auto command = first;
  while (command != nullptr)
  {
    auto next = command->GetNext();
    command->Execute(cmdbuf);
    command->~Command();
    command = next;
  }
  command_offset = 0;
  first = nullptr;
  last = nullptr;
}

void Scheduler::AcquireNewChunk()
{
  std::scoped_lock lock{m_reserve_mutex};
  if (m_chunk_reserve.empty())
  {
    m_chunk = std::make_unique<CommandChunk>();
    return;
  }
  m_chunk = std::move(m_chunk_reserve.back());
  m_chunk_reserve.pop_back();
}

void Scheduler::Flush()
{
  if (m_chunk->Empty())
    return;

  {
    std::scoped_lock lock{m_work_mutex};
    m_work_queue.push(std::move(m_chunk));
    m_submit_loop->Wakeup();
  }
  AcquireNewChunk();
}

void Scheduler::SyncWorker()
{
  if (!m_use_worker_thread)
  {
    return;
  }
  Flush();
  m_submit_loop->Wait();
}

void Scheduler::WorkerThread()
{
  m_submit_loop->Run([this]() {
    while (true)
    {
      std::unique_ptr<CommandChunk> work;
      {
        std::scoped_lock lock{m_work_mutex};
        if (m_work_queue.empty())
        {
          m_submit_loop->AllowSleep();
          return;
        }
        work = std::move(m_work_queue.front());
        m_work_queue.pop();
      }

      work->ExecuteAll(m_command_buffer_manager.get());
      std::scoped_lock reserve_lock{m_reserve_mutex};
      m_chunk_reserve.push_back(std::move(work));
    }
  });
}

void Scheduler::Shutdown()
{
  SyncWorker();
  SynchronizeSubmissionThread();

  u64 fence_counter = m_semaphore->GetCurrentFenceCounter();
  if (fence_counter != 0)
    m_semaphore->WaitForFenceCounter(fence_counter - 1);

  m_command_buffer_manager->Shutdown();
}

void Scheduler::SynchronizeSubmissionThread()
{
  SyncWorker();
  m_command_buffer_manager->WaitForSubmitWorkerThreadIdle();
}

void Scheduler::WaitForFenceCounter(u64 counter)
{
  if (m_semaphore->GetCompletedFenceCounter() >= counter)
    return;

  SyncWorker();
  m_semaphore->WaitForFenceCounter(counter);
}

void Scheduler::SubmitCommandBuffer(bool submit_on_worker_thread, bool wait_for_completion,
                                    VkSwapchainKHR present_swap_chain, uint32_t present_image_index)
{
  const u64 fence_counter = m_semaphore->BumpNextFenceCounter();
  Record([fence_counter, submit_on_worker_thread, wait_for_completion, present_swap_chain,
          present_image_index](CommandBufferManager* command_buffer_mgr) {
    command_buffer_mgr->GetStateTracker()->EndRenderPass();
    command_buffer_mgr->SubmitCommandBuffer(fence_counter, submit_on_worker_thread,
                                            wait_for_completion, present_swap_chain,
                                            present_image_index);
  });

  if (wait_for_completion) [[unlikely]]
    g_scheduler->WaitForFenceCounter(fence_counter);
  else
    Flush();
}

u64 Scheduler::GetCompletedFenceCounter() const
{
  return m_semaphore->GetCompletedFenceCounter();
}

u64 Scheduler::GetCurrentFenceCounter() const
{
  return m_semaphore->GetCurrentFenceCounter();
}

std::unique_ptr<Scheduler> g_scheduler;
}  // namespace Vulkan

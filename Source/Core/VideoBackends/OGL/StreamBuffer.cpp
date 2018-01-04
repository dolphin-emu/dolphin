// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/OGL/StreamBuffer.h"

#include "Common/Align.h"
#include "Common/CommonFuncs.h"
#include "Common/GL/GLUtil.h"
#include "Common/MemoryUtil.h"

#include "VideoBackends/OGL/Render.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace OGL
{
// moved out of constructor, so m_buffer is allowed to be const
static u32 GenBuffer()
{
  u32 id;
  glGenBuffers(1, &id);
  return id;
}

StreamBuffer::StreamBuffer(u32 type, u32 size)
    : m_buffer(GenBuffer()), m_buffertype(type), m_size(ROUND_UP_POW2(size)),
      m_bit_per_slot(IntLog2(ROUND_UP_POW2(size) / SYNC_POINTS))
{
  m_iterator = 0;
  m_used_iterator = 0;
  m_free_iterator = 0;
}

StreamBuffer::~StreamBuffer()
{
  glDeleteBuffers(1, &m_buffer);
}

/* Shared synchronization code for ring buffers
 *
 * The next three functions are to create/delete/use the OpenGL synchronization.
 * ARB_sync (OpenGL 3.2) is used and required.
 *
 * To reduce overhead, the complete buffer is splitted up into SYNC_POINTS chunks.
 * For each of this chunks, there is a fence which checks if this chunk is still in use.
 *
 * As our API allows to alloc more memory then it has to use, we have to catch how much is already
 * written.
 *
 * m_iterator      - writing position
 * m_free_iterator - last position checked if free
 * m_used_iterator - last position known to be written
 *
 * So on alloc, we have to wait for all slots between m_free_iterator and m_iterator (and set
 * m_free_iterator to m_iterator afterwards).
 *
 * We also assume that this buffer is accessed by the GPU between the Unmap and Map function,
 * so we may create the fences on the start of mapping.
 * Some here, new fences for the chunks between m_used_iterator and m_iterator (also update
 * m_used_iterator).
 *
 * As ring buffers have an ugly behavior on rollover, have fun to read this code ;)
 */

void StreamBuffer::CreateFences()
{
  for (int i = 0; i < SYNC_POINTS; i++)
  {
    m_fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  }
}
void StreamBuffer::DeleteFences()
{
  for (int i = Slot(m_free_iterator) + 1; i < SYNC_POINTS; i++)
  {
    glDeleteSync(m_fences[i]);
  }
  for (int i = 0; i < Slot(m_iterator); i++)
  {
    glDeleteSync(m_fences[i]);
  }
}
void StreamBuffer::AllocMemory(u32 size)
{
  // insert waiting slots for used memory
  for (int i = Slot(m_used_iterator); i < Slot(m_iterator); i++)
  {
    m_fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  }
  m_used_iterator = m_iterator;

  // wait for new slots to end of buffer
  for (int i = Slot(m_free_iterator) + 1; i <= Slot(m_iterator + size) && i < SYNC_POINTS; i++)
  {
    glClientWaitSync(m_fences[i], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
    glDeleteSync(m_fences[i]);
  }
  m_free_iterator = m_iterator + size;

  // if buffer is full
  if (m_iterator + size >= m_size)
  {
    // insert waiting slots in unused space at the end of the buffer
    for (int i = Slot(m_used_iterator); i < SYNC_POINTS; i++)
    {
      m_fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    // move to the start
    m_used_iterator = m_iterator = 0;  // offset 0 is always aligned

    // wait for space at the start
    for (int i = 0; i <= Slot(m_iterator + size); i++)
    {
      glClientWaitSync(m_fences[i], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
      glDeleteSync(m_fences[i]);
    }
    m_free_iterator = m_iterator + size;
  }
}

/* The usual way to stream data to the GPU.
 * Described here: https://www.opengl.org/wiki/Buffer_Object_Streaming#Unsynchronized_buffer_mapping
 * Just do unsync appends until the buffer is full.
 * When it's full, orphan (alloc a new buffer and free the old one)
 *
 * As reallocation is an overhead, this method isn't as fast as it is known to be.
 */
class MapAndOrphan : public StreamBuffer
{
public:
  MapAndOrphan(u32 type, u32 size) : StreamBuffer(type, size)
  {
    glBindBuffer(m_buffertype, m_buffer);
    glBufferData(m_buffertype, m_size, nullptr, GL_STREAM_DRAW);
  }

  ~MapAndOrphan() {}
  std::pair<u8*, u32> Map(u32 size) override
  {
    if (m_iterator + size >= m_size)
    {
      glBufferData(m_buffertype, m_size, nullptr, GL_STREAM_DRAW);
      m_iterator = 0;
    }
    u8* pointer = (u8*)glMapBufferRange(m_buffertype, m_iterator, size,
                                        GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT |
                                            GL_MAP_UNSYNCHRONIZED_BIT);
    return std::make_pair(pointer, m_iterator);
  }

  void Unmap(u32 used_size) override
  {
    glFlushMappedBufferRange(m_buffertype, 0, used_size);
    glUnmapBuffer(m_buffertype);
    m_iterator += used_size;
  }
};

/* A modified streaming way without reallocation
 * This one fixes the reallocation overhead of the MapAndOrphan one.
 * So it alloc a ring buffer on initialization.
 * But with this limited resource, we have to care about the CPU-GPU distance.
 * Else this fifo may overflow.
 * So we had traded orphan vs syncing.
 */
class MapAndSync : public StreamBuffer
{
public:
  MapAndSync(u32 type, u32 size) : StreamBuffer(type, size)
  {
    CreateFences();
    glBindBuffer(m_buffertype, m_buffer);
    glBufferData(m_buffertype, m_size, nullptr, GL_STREAM_DRAW);
  }

  ~MapAndSync() { DeleteFences(); }
  std::pair<u8*, u32> Map(u32 size) override
  {
    AllocMemory(size);
    u8* pointer = (u8*)glMapBufferRange(m_buffertype, m_iterator, size,
                                        GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT |
                                            GL_MAP_UNSYNCHRONIZED_BIT);
    return std::make_pair(pointer, m_iterator);
  }

  void Unmap(u32 used_size) override
  {
    glFlushMappedBufferRange(m_buffertype, 0, used_size);
    glUnmapBuffer(m_buffertype);
    m_iterator += used_size;
  }
};

/* Streaming fifo without mapping overhead.
 * This one usually requires ARB_buffer_storage (OpenGL 4.4).
 * And is usually not available on OpenGL3 GPUs.
 *
 * ARB_buffer_storage allows us to render from a mapped buffer.
 * So we map it persistently in the initialization.
 *
 * Unsync mapping sounds like an easy task, but it isn't for threaded drivers.
 * So every mapping on current close-source driver _will_ end in
 * at least a round trip time between two threads.
 *
 * As persistently mapped buffer can't use orphaning, we also have to sync.
 */
class BufferStorage : public StreamBuffer
{
public:
  BufferStorage(u32 type, u32 size, bool _coherent = false)
      : StreamBuffer(type, size), coherent(_coherent)
  {
    CreateFences();
    glBindBuffer(m_buffertype, m_buffer);

    // PERSISTANT_BIT to make sure that the buffer can be used while mapped
    // COHERENT_BIT is set so we don't have to use a MemoryBarrier on write
    // CLIENT_STORAGE_BIT is set since we access the buffer more frequently on the client side then
    // server side
    glBufferStorage(m_buffertype, m_size, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT |
                                                       (coherent ? GL_MAP_COHERENT_BIT : 0));
    m_pointer = (u8*)glMapBufferRange(
        m_buffertype, 0, m_size, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT |
                                     (coherent ? GL_MAP_COHERENT_BIT : GL_MAP_FLUSH_EXPLICIT_BIT));
  }

  ~BufferStorage()
  {
    DeleteFences();
    glUnmapBuffer(m_buffertype);
    glBindBuffer(m_buffertype, 0);
  }

  std::pair<u8*, u32> Map(u32 size) override
  {
    AllocMemory(size);
    return std::make_pair(m_pointer + m_iterator, m_iterator);
  }

  void Unmap(u32 used_size) override
  {
    if (!coherent)
      glFlushMappedBufferRange(m_buffertype, m_iterator, used_size);
    m_iterator += used_size;
  }

  u8* m_pointer;
  const bool coherent;
};

/* --- AMD only ---
 * Another streaming fifo without mapping overhead.
 * As we can't orphan without mapping, we have to sync.
 *
 * This one uses AMD_pinned_memory which is available on all AMD GPUs.
 * OpenGL 4.4 drivers should use BufferStorage.
 */
class PinnedMemory : public StreamBuffer
{
public:
  PinnedMemory(u32 type, u32 size) : StreamBuffer(type, size)
  {
    CreateFences();
    m_pointer = static_cast<u8*>(Common::AllocateAlignedMemory(
        Common::AlignUp(m_size, ALIGN_PINNED_MEMORY), ALIGN_PINNED_MEMORY));
    glBindBuffer(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, m_buffer);
    glBufferData(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD,
                 Common::AlignUp(m_size, ALIGN_PINNED_MEMORY), m_pointer, GL_STREAM_COPY);
    glBindBuffer(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, 0);
    glBindBuffer(m_buffertype, m_buffer);
  }

  ~PinnedMemory()
  {
    DeleteFences();
    glBindBuffer(m_buffertype, 0);
    glFinish();  // ogl pipeline must be flushed, else this buffer can be in use
    Common::FreeAlignedMemory(m_pointer);
    m_pointer = nullptr;
  }

  std::pair<u8*, u32> Map(u32 size) override
  {
    AllocMemory(size);
    return std::make_pair(m_pointer + m_iterator, m_iterator);
  }

  void Unmap(u32 used_size) override { m_iterator += used_size; }
  u8* m_pointer;
  static const u32 ALIGN_PINNED_MEMORY = 4096;
};

/* Fifo based on the glBufferSubData call.
 * As everything must be copied before glBufferSubData returns,
 * an additional memcpy in the driver will be done.
 * So this is a huge overhead, only use it if required.
 */
class BufferSubData : public StreamBuffer
{
public:
  BufferSubData(u32 type, u32 size) : StreamBuffer(type, size)
  {
    glBindBuffer(m_buffertype, m_buffer);
    glBufferData(m_buffertype, size, nullptr, GL_STATIC_DRAW);
    m_pointer = new u8[m_size];
  }

  ~BufferSubData() { delete[] m_pointer; }
  std::pair<u8*, u32> Map(u32 size) override { return std::make_pair(m_pointer, 0); }
  void Unmap(u32 used_size) override { glBufferSubData(m_buffertype, 0, used_size, m_pointer); }
  u8* m_pointer;
};

/* Fifo based on the glBufferData call.
 * Some trashy drivers stall in BufferSubData.
 * So here we use glBufferData, which realloc this buffer every time.
 * This may avoid stalls, but it is a bigger overhead than BufferSubData.
 */
class BufferData : public StreamBuffer
{
public:
  BufferData(u32 type, u32 size) : StreamBuffer(type, size)
  {
    glBindBuffer(m_buffertype, m_buffer);
    m_pointer = new u8[m_size];
  }

  ~BufferData() { delete[] m_pointer; }
  std::pair<u8*, u32> Map(u32 size) override { return std::make_pair(m_pointer, 0); }
  void Unmap(u32 used_size) override
  {
    glBufferData(m_buffertype, used_size, m_pointer, GL_STREAM_DRAW);
  }

  u8* m_pointer;
};

// Chooses the best streaming method based on the supported extensions and known issues
std::unique_ptr<StreamBuffer> StreamBuffer::Create(u32 type, u32 size)
{
  // without basevertex support, only streaming methods whith uploads everything to zero works fine:
  if (!g_ogl_config.bSupportsGLBaseVertex)
  {
    if (!DriverDetails::HasBug(DriverDetails::BUG_BROKEN_BUFFER_STREAM))
      return std::make_unique<BufferSubData>(type, size);

    // BufferData is by far the worst way, only use it if needed
    return std::make_unique<BufferData>(type, size);
  }

  // Prefer the syncing buffers over the orphaning one
  if (g_ogl_config.bSupportsGLSync)
  {
    // pinned memory is much faster than buffer storage on AMD cards
    if (g_ogl_config.bSupportsGLPinnedMemory &&
        !(DriverDetails::HasBug(DriverDetails::BUG_BROKEN_PINNED_MEMORY)))
      return std::make_unique<PinnedMemory>(type, size);

    // buffer storage works well in most situations
    bool coherent = DriverDetails::HasBug(DriverDetails::BUG_BROKEN_EXPLICIT_FLUSH);
    if (g_ogl_config.bSupportsGLBufferStorage &&
        !(DriverDetails::HasBug(DriverDetails::BUG_BROKEN_BUFFER_STORAGE) &&
          type == GL_ARRAY_BUFFER) &&
        !(DriverDetails::HasBug(DriverDetails::BUG_INTEL_BROKEN_BUFFER_STORAGE) &&
          type == GL_ELEMENT_ARRAY_BUFFER))
      return std::make_unique<BufferStorage>(type, size, coherent);

    // don't fall back to MapAnd* for Nvidia drivers
    if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_UNSYNC_MAPPING))
      return std::make_unique<BufferSubData>(type, size);

    // mapping fallback
    if (g_ogl_config.bSupportsGLSync)
      return std::make_unique<MapAndSync>(type, size);
  }

  // default fallback, should work everywhere, but isn't the best way to do this job
  return std::make_unique<MapAndOrphan>(type, size);
}
}

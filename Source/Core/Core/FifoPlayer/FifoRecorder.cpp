// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/FifoPlayer/FifoRecorder.h"

#include <algorithm>
#include <cstring>

#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Thread.h"

#include "Core/HW/Memmap.h"
#include "Core/System.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoEvents.h"
#include "VideoCommon/XFMemory.h"
#include "VideoCommon/XFStructs.h"

class FifoRecorder::FifoRecordAnalyzer : public OpcodeDecoder::Callback
{
public:
  explicit FifoRecordAnalyzer(FifoRecorder* owner) : m_owner(owner) {}
  explicit FifoRecordAnalyzer(FifoRecorder* owner, const u32* cpmem)
      : m_owner(owner), m_cpmem(cpmem)
  {
  }

  OPCODE_CALLBACK(void OnXF(u16 address, u8 count, const u8* data)) {}
  OPCODE_CALLBACK(void OnCP(u8 command, u32 value)) { GetCPState().LoadCPReg(command, value); }
  OPCODE_CALLBACK(void OnBP(u8 command, u32 value)) {}
  OPCODE_CALLBACK(void OnIndexedLoad(CPArray array, u32 index, u16 address, u8 size));
  OPCODE_CALLBACK(void OnPrimitiveCommand(OpcodeDecoder::Primitive primitive, u8 vat,
                                          u32 vertex_size, u16 num_vertices,
                                          const u8* vertex_data));
  OPCODE_CALLBACK(void OnDisplayList(u32 address, u32 size));
  OPCODE_CALLBACK(void OnNop(u32 count)) {}
  OPCODE_CALLBACK(void OnUnknown(u8 opcode, const u8* data)) {}

  OPCODE_CALLBACK(void OnCommand(const u8* data, u32 size)) {}

  OPCODE_CALLBACK(CPState& GetCPState()) { return m_cpmem; }

  OPCODE_CALLBACK(u32 GetVertexSize(u8 vat))
  {
    return VertexLoaderBase::GetVertexSize(GetCPState().vtx_desc, GetCPState().vtx_attr[vat]);
  }

private:
  void ProcessVertexComponent(CPArray array_index, VertexComponentFormat array_type,
                              u32 component_offset, u32 component_size, u32 vertex_size,
                              u16 num_vertices, const u8* vertex_data, u32 byte_offset = 0);

  FifoRecorder* const m_owner;
  CPState m_cpmem;
};

void FifoRecorder::FifoRecordAnalyzer::OnIndexedLoad(CPArray array, u32 index, u16 address, u8 size)
{
  const u32 load_address = m_cpmem.array_bases[array] + m_cpmem.array_strides[array] * index;

  m_owner->UseMemory(load_address, size * sizeof(u32), MemoryUpdate::Type::XFData);
}

// TODO: The following code is copied with modifications from VertexLoaderBase.
// Surely there's a better solution?
#include "VideoCommon/VertexLoader_Color.h"
#include "VideoCommon/VertexLoader_Normal.h"
#include "VideoCommon/VertexLoader_Position.h"
#include "VideoCommon/VertexLoader_TextCoord.h"

void FifoRecorder::FifoRecordAnalyzer::OnPrimitiveCommand(OpcodeDecoder::Primitive primitive,
                                                          u8 vat, u32 vertex_size, u16 num_vertices,
                                                          const u8* vertex_data)
{
  const auto& vtx_desc = m_cpmem.vtx_desc;
  const auto& vtx_attr = m_cpmem.vtx_attr[vat];

  u32 offset = 0;

  if (vtx_desc.low.PosMatIdx)
    offset++;
  for (auto texmtxidx : vtx_desc.low.TexMatIdx)
  {
    if (texmtxidx)
      offset++;
  }
  const u32 pos_size = VertexLoader_Position::GetSize(vtx_desc.low.Position, vtx_attr.g0.PosFormat,
                                                      vtx_attr.g0.PosElements);
  const u32 pos_direct_size = VertexLoader_Position::GetSize(
      VertexComponentFormat::Direct, vtx_attr.g0.PosFormat, vtx_attr.g0.PosElements);
  ProcessVertexComponent(CPArray::Position, vtx_desc.low.Position, offset, pos_direct_size,
                         vertex_size, num_vertices, vertex_data);
  offset += pos_size;

  const u32 norm_size =
      VertexLoader_Normal::GetSize(vtx_desc.low.Normal, vtx_attr.g0.NormalFormat,
                                   vtx_attr.g0.NormalElements, vtx_attr.g0.NormalIndex3);
  const u32 norm_direct_size =
      VertexLoader_Normal::GetSize(VertexComponentFormat::Direct, vtx_attr.g0.NormalFormat,
                                   vtx_attr.g0.NormalElements, vtx_attr.g0.NormalIndex3);
  if (vtx_attr.g0.NormalIndex3 && IsIndexed(vtx_desc.low.Normal) &&
      vtx_attr.g0.NormalElements == NormalComponentCount::NTB)
  {
    // We're in 3-index mode, and we're using an indexed format and have the
    // normal/tangent/binormal, so we actually need to deal with 3-index mode.
    const u32 index_size = vtx_desc.low.Normal == VertexComponentFormat::Index16 ? 2 : 1;
    ASSERT(norm_size == index_size * 3);
    // 3-index mode uses one index each for the normal, tangent and binormal;
    // the tangent and binormal are internally offset.
    // The offset is based on the component size, not to the index itself;
    // for instance, with 32-bit float normals, each normal vector is 3*sizeof(float) = 12 bytes,
    // so the normal vector is offset by 0 bytes, the tangent by 12, and the binormal by 24.
    // Using a byte offset instead of increasing the index means that using the same index for all
    // elements is the same as not using the 3-index mode (increasing the index would give differing
    // results if the normal array's stride was something other than 12, for instance if vertices
    // were contiguous in main memory instead of individual components being used).
    const u32 element_size = GetElementSize(vtx_attr.g0.NormalFormat) * 3;
    ProcessVertexComponent(CPArray::Normal, vtx_desc.low.Normal, offset, element_size, vertex_size,
                           num_vertices, vertex_data);
    ProcessVertexComponent(CPArray::Normal, vtx_desc.low.Normal, offset + index_size, element_size,
                           vertex_size, num_vertices, vertex_data, element_size);
    ProcessVertexComponent(CPArray::Normal, vtx_desc.low.Normal, offset + 2 * index_size,
                           element_size, vertex_size, num_vertices, vertex_data, 2 * element_size);
  }
  else
  {
    ProcessVertexComponent(CPArray::Normal, vtx_desc.low.Normal, offset, norm_direct_size,
                           vertex_size, num_vertices, vertex_data);
  }
  offset += norm_size;

  for (u32 i = 0; i < vtx_desc.low.Color.Size(); i++)
  {
    const u32 color_size =
        VertexLoader_Color::GetSize(vtx_desc.low.Color[i], vtx_attr.GetColorFormat(i));
    const u32 color_direct_size =
        VertexLoader_Color::GetSize(VertexComponentFormat::Direct, vtx_attr.GetColorFormat(i));
    ProcessVertexComponent(CPArray::Color0 + i, vtx_desc.low.Color[i], offset, color_direct_size,
                           vertex_size, num_vertices, vertex_data);
    offset += color_size;
  }
  for (u32 i = 0; i < vtx_desc.high.TexCoord.Size(); i++)
  {
    const u32 tc_size = VertexLoader_TextCoord::GetSize(
        vtx_desc.high.TexCoord[i], vtx_attr.GetTexFormat(i), vtx_attr.GetTexElements(i));
    const u32 tc_direct_size = VertexLoader_TextCoord::GetSize(
        VertexComponentFormat::Direct, vtx_attr.GetTexFormat(i), vtx_attr.GetTexElements(i));
    ProcessVertexComponent(CPArray::TexCoord0 + i, vtx_desc.high.TexCoord[i], offset,
                           tc_direct_size, vertex_size, num_vertices, vertex_data);
    offset += tc_size;
  }

  ASSERT(offset == vertex_size);
}

// If a component is indexed, the array it indexes into for data must be saved.
void FifoRecorder::FifoRecordAnalyzer::ProcessVertexComponent(
    CPArray array_index, VertexComponentFormat array_type, u32 component_offset, u32 component_size,
    u32 vertex_size, u16 num_vertices, const u8* vertex_data, u32 byte_offset)
{
  // Skip if not indexed array
  if (!IsIndexed(array_type))
    return;

  u16 max_index = 0;

  // Determine min and max indices
  if (array_type == VertexComponentFormat::Index8)
  {
    for (u16 vertex_num = 0; vertex_num < num_vertices; vertex_num++)
    {
      const u8 index = vertex_data[component_offset];
      vertex_data += vertex_size;

      // 0xff skips the vertex
      if (index != 0xff)
      {
        if (index > max_index)
          max_index = index;
      }
    }
  }
  else
  {
    for (u16 vertex_num = 0; vertex_num < num_vertices; vertex_num++)
    {
      const u16 index = Common::swap16(&vertex_data[component_offset]);
      vertex_data += vertex_size;

      // 0xffff skips the vertex
      if (index != 0xffff)
      {
        if (index > max_index)
          max_index = index;
      }
    }
  }

  const u32 array_start = m_cpmem.array_bases[array_index] + byte_offset;
  const u32 array_size = m_cpmem.array_strides[array_index] * max_index + component_size;

  m_owner->UseMemory(array_start, array_size, MemoryUpdate::Type::VertexStream);
}

void FifoRecorder::FifoRecordAnalyzer::OnDisplayList(u32 address, u32 size)
{
  m_owner->UseMemory(address, size, MemoryUpdate::Type::DisplayList);
  // OpcodeDecoding will call WriteGPCommand for the contents of the display list, so we don't need
  // to process it here.
}

FifoRecorder::FifoRecorder(Core::System& system) : m_system(system)
{
}

FifoRecorder::~FifoRecorder() = default;

void FifoRecorder::StartRecording(s32 numFrames, CallbackFunc finishedCb)
{
  std::lock_guard lk(m_mutex);

  m_File = std::make_unique<FifoDataFile>();

  // TODO: This, ideally, would be deallocated when done recording.
  //       However, care needs to be taken since global state
  //       and multithreading don't play well nicely together.
  //       The video thread may call into functions that utilize these
  //       despite 'end recording' being requested via StopRecording().
  //       (e.g. OpcodeDecoder calling UseMemory())
  //
  // Basically:
  //   - Singletons suck
  //   - Global variables suck
  //   - Multithreading with the above two sucks
  //
  auto& memory = m_system.GetMemory();
  m_Ram.resize(memory.GetRamSize());
  m_ExRam.resize(memory.GetExRamSize());

  std::fill(m_Ram.begin(), m_Ram.end(), 0);
  std::fill(m_ExRam.begin(), m_ExRam.end(), 0);

  m_File->SetIsWii(m_system.IsWii());

  if (!m_IsRecording)
  {
    m_WasRecording = false;
    m_IsRecording = true;
    m_RecordFramesRemaining = numFrames;
  }

  m_RequestedRecordingEnd = false;
  m_FinishedCb = finishedCb;

  m_end_of_frame_event = AfterFrameEvent::Register(
      [this](const Core::System& system) {
        const bool was_recording = OpcodeDecoder::g_record_fifo_data;
        OpcodeDecoder::g_record_fifo_data = IsRecording();

        if (!OpcodeDecoder::g_record_fifo_data)
        {
          // Remove this frame end callback when recording finishes
          m_end_of_frame_event.reset();
          return;
        }

        if (!was_recording)
        {
          RecordInitialVideoMemory();
        }

        const auto& fifo = system.GetCommandProcessor().GetFifo();
        EndFrame(fifo.CPBase.load(std::memory_order_relaxed),
                 fifo.CPEnd.load(std::memory_order_relaxed));
      },
      "FifoRecorder::EndFrame");
}

void FifoRecorder::RecordInitialVideoMemory()
{
  const u32* bpmem_ptr = reinterpret_cast<const u32*>(&bpmem);
  u32 cpmem[256] = {};
  // The FIFO recording format splits XF memory into xfmem and xfregs; follow
  // that split here.
  const u32* xfmem_ptr = reinterpret_cast<const u32*>(&xfmem);
  const u32* xfregs_ptr = reinterpret_cast<const u32*>(&xfmem) + FifoDataFile::XF_MEM_SIZE;
  u32 xfregs_size = sizeof(XFMemory) / 4 - FifoDataFile::XF_MEM_SIZE;

  g_main_cp_state.FillCPMemoryArray(cpmem);

  SetVideoMemory(bpmem_ptr, cpmem, xfmem_ptr, xfregs_ptr, xfregs_size, s_tex_mem.data());
}

void FifoRecorder::StopRecording()
{
  std::lock_guard lk(m_mutex);
  m_RequestedRecordingEnd = true;
}

bool FifoRecorder::IsRecordingDone() const
{
  return m_WasRecording && m_File != nullptr;
}

FifoDataFile* FifoRecorder::GetRecordedFile() const
{
  return m_File.get();
}

void FifoRecorder::WriteGPCommand(const u8* data, u32 size, bool in_display_list)
{
  if (!m_SkipNextData)
  {
    // Assumes data contains all information for the command
    // Calls FifoRecorder::UseMemory
    const u32 analyzed_size = OpcodeDecoder::RunCommand(data, size, *m_record_analyzer);

    // Make sure FifoPlayer's command analyzer agrees about the size of the command.
    if (analyzed_size != size)
    {
      PanicAlertFmt("FifoRecorder: Expected command to be {} bytes long, we were given {} bytes",
                    analyzed_size, size);
    }

    // Copy data to buffer (display lists are recorded by FifoRecordAnalyzer::OnDisplayList and
    // do not need to be inlined)
    if (!in_display_list)
    {
      size_t currentSize = m_FifoData.size();
      m_FifoData.resize(currentSize + size);
      memcpy(&m_FifoData[currentSize], data, size);
    }
  }

  if (m_FrameEnded && !m_FifoData.empty())
  {
    m_CurrentFrame.fifoData = m_FifoData;

    {
      std::lock_guard lk(m_mutex);

      // Copy frame to file
      // The file will be responsible for freeing the memory allocated for each frame's fifoData
      m_File->AddFrame(m_CurrentFrame);

      if (m_FinishedCb && m_RequestedRecordingEnd)
        m_FinishedCb();
    }

    m_CurrentFrame.memoryUpdates.clear();
    m_FifoData.clear();
    m_FrameEnded = false;
  }

  m_SkipNextData = m_SkipFutureData;
}

void FifoRecorder::UseMemory(u32 address, u32 size, MemoryUpdate::Type type, bool dynamicUpdate)
{
  auto& memory = m_system.GetMemory();

  u8* curData;
  if (address & 0x10000000)
  {
    curData = &m_ExRam[address & memory.GetExRamMask()];
  }
  else
  {
    curData = &m_Ram[address & memory.GetRamMask()];
  }
  u8* newData = memory.GetPointerForRange(address, size);

  if (!dynamicUpdate && memcmp(curData, newData, size) != 0)
  {
    // Update current memory
    memcpy(curData, newData, size);

    // Record memory update
    MemoryUpdate memUpdate;
    memUpdate.address = address;
    memUpdate.fifoPosition = (u32)(m_FifoData.size());
    memUpdate.type = type;
    memUpdate.data.resize(size);
    std::copy(newData, newData + size, memUpdate.data.begin());

    m_CurrentFrame.memoryUpdates.push_back(std::move(memUpdate));
  }
  else if (dynamicUpdate)
  {
    // Shadow the data so it won't be recorded as changed by a future UseMemory
    memcpy(curData, newData, size);
  }
}

void FifoRecorder::EndFrame(u32 fifoStart, u32 fifoEnd)
{
  // m_IsRecording is assumed to be true at this point, otherwise this function would not be called
  std::lock_guard lk(m_mutex);

  m_FrameEnded = true;

  m_CurrentFrame.fifoStart = fifoStart;
  m_CurrentFrame.fifoEnd = fifoEnd;

  if (m_WasRecording)
  {
    // If recording a fixed number of frames then check if the end of the recording was reached
    if (m_RecordFramesRemaining > 0)
    {
      --m_RecordFramesRemaining;
      if (m_RecordFramesRemaining == 0)
        m_RequestedRecordingEnd = true;
    }
  }
  else
  {
    m_WasRecording = true;

    // Skip the first data which will be the frame copy command
    m_SkipNextData = true;
    m_SkipFutureData = false;

    m_FrameEnded = false;

    m_FifoData.reserve(1024 * 1024 * 4);
    m_FifoData.clear();
  }

  if (m_RequestedRecordingEnd)
  {
    // Skip data after the next time WriteFifoData is called
    m_SkipFutureData = true;
    // Signal video backend that it should not call this function when the next frame ends
    m_IsRecording = false;

    size_t fifo_bytes = 0;
    size_t update_bytes = 0;
    size_t xf_bytes = 0;
    size_t tex_bytes = 0;
    size_t vert_bytes = 0;
    size_t tmem_bytes = 0;
    size_t dl_bytes = 0;
    size_t update_overhead = 0;

    for (u32 i = 0; i < m_File->GetFrameCount(); i++)
    {
      auto frame = m_File->GetFrame(i);
      fifo_bytes += frame.fifoData.size();

      for (auto& update : frame.memoryUpdates)
      {
        update_bytes += update.data.size() + 24;
        update_overhead += 24;
        switch (update.type)
        {
        case MemoryUpdate::Type::TextureMap:
          tex_bytes += update.data.size() + 24;
          break;
        case MemoryUpdate::Type::XFData:
          xf_bytes += update.data.size() + 24;
          break;
        case MemoryUpdate::Type::VertexStream:
          vert_bytes += update.data.size() + 24;
          break;
        case MemoryUpdate::Type::TMEM:
          tmem_bytes += update.data.size() + 24;
          break;
        case MemoryUpdate::Type::DisplayList:
          dl_bytes += update.data.size() + 24;
          break;
        }
      }
    }

    fmt::print("FifoBytes: {}\n", fifo_bytes);
    fmt::print("Updates: {}\n", update_bytes);
    fmt::print("TexBytes: {}\n", tex_bytes);
    fmt::print("XfBytes: {}\n", xf_bytes);
    fmt::print("VertBytes: {}\n", vert_bytes);
    fmt::print("TmemBytes: {}\n", tmem_bytes);
    fmt::print("DlBytes: {}\n", dl_bytes);
    fmt::print("Overhead: {}\n", update_overhead);
  }
}

void FifoRecorder::SetVideoMemory(const u32* bpMem, const u32* cpMem, const u32* xfMem,
                                  const u32* xfRegs, u32 xfRegsSize, const u8* texMem_ptr)
{
  std::lock_guard lk(m_mutex);

  if (m_File)
  {
    memcpy(m_File->GetBPMem(), bpMem, FifoDataFile::BP_MEM_SIZE * 4);
    memcpy(m_File->GetCPMem(), cpMem, FifoDataFile::CP_MEM_SIZE * 4);
    memcpy(m_File->GetXFMem(), xfMem, FifoDataFile::XF_MEM_SIZE * 4);

    u32 xfRegsCopySize = std::min((u32)FifoDataFile::XF_REGS_SIZE, xfRegsSize);
    memcpy(m_File->GetXFRegs(), xfRegs, xfRegsCopySize * 4);

    memcpy(m_File->GetTexMem(), texMem_ptr, FifoDataFile::TEX_MEM_SIZE);
  }

  m_record_analyzer = std::make_unique<FifoRecordAnalyzer>(this, cpMem);
}

bool FifoRecorder::IsRecording() const
{
  return m_IsRecording;
}

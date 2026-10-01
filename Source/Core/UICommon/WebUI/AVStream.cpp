// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/WebUI/AVStream.h"

#include <rtc/rtc.hpp>

#include <picojson.h>

#include "Common/Config/Config.h"
#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/SPSCQueue.h"

#include "Core/Config/MainSettings.h"

#include "UICommon/WebUI/AVEncoder.h"

namespace
{

// A container of `weak_ptr` that many threads can append items to.
template <typename T>
class ManyProducerWeakVector
{
public:
  // May be called concurrently.
  void Append(std::weak_ptr<T> track)
  {
    std::lock_guard lk{m_producer_mutex};
    m_new_items.Push(std::move(track));
  }

  // Only safe from a single thread.
  void ForEach(std::invocable<T&> auto&& callback)
  {
    for (std::weak_ptr<T> item; m_new_items.Pop(item);)
      m_items.emplace_back(std::move(item));

    std::erase_if(m_items, [&](auto&& ptr) {
      if (const auto sptr = ptr.lock())
      {
        callback(*sptr);
        return false;
      }
      return true;
    });
  }

private:
  std::mutex m_producer_mutex;
  Common::SPSCQueue<std::weak_ptr<T>> m_new_items;
  std::vector<std::weak_ptr<T>> m_items;
};

}  // namespace

namespace WebUI
{

struct AVStreamPeer::Impl
{
  void CreatePeerConnection();
  void SendMessage(const std::string& message);

  std::shared_ptr<AVStream::Impl> m_av_stream_impl;

  std::shared_ptr<rtc::PeerConnection> m_peer_connection;

  AVStreamPeer::SendCallbackType m_send_callback;
  AVStreamPeer::ControlCallbackType m_control_callback;
};

struct AVStream::Impl
{
  void SendAudioDataToAllPeers(std::span<const u8> data, s64 pts);
  void SendVideoDataToAllPeers(std::span<const u8> data, s64 pts);

  // AVStreamPeer-provided channels to send AV packets to.
  ManyProducerWeakVector<rtc::Track> m_audio_tracks;
  ManyProducerWeakVector<rtc::Track> m_video_tracks;
};

AVStreamPeer::AVStreamPeer(std::shared_ptr<AVStream> av_stream)
    : m_impl{std::make_unique<Impl>(av_stream->m_impl)}, m_av_stream{std::move(av_stream)}
{
}

AVStreamPeer::~AVStreamPeer() = default;

void AVStreamPeer::Impl::SendMessage(const std::string& message)
{
  m_send_callback(message);
}

void AVStreamPeer::Impl::CreatePeerConnection()
{
  rtc::Configuration config{
      .enableIceTcp = true,
      .disableAutoNegotiation = true,
  };

  for (const auto& ice_server :
       SplitString(Config::Get(Config::MAIN_WEB_INTERFACE_ICE_SERVERS), ' '))
  {
    INFO_LOG_FMT(COMMON, "AVStreamPeer: Adding ICE server: {}", ice_server);
    config.iceServers.emplace_back(ice_server);
  }

  m_peer_connection = std::make_shared<rtc::PeerConnection>(config);

  auto& pc = m_peer_connection;

  pc->onLocalDescription([this](const rtc::Description& description) {
    picojson::object obj;
    obj.emplace("type", description.typeString());
    obj.emplace("id", "server");
    obj.emplace("description", description);
    obj.emplace("iceServers", Config::Get(Config::MAIN_WEB_INTERFACE_ICE_SERVERS));
    SendMessage(picojson::value(std::move(obj)).serialize());
  });

  pc->onLocalCandidate([this](const rtc::Candidate& candidate) {
    picojson::object obj;
    obj.emplace("type", "candidate");
    obj.emplace("candidate", candidate.candidate());
    obj.emplace("mid", candidate.mid());
    SendMessage(picojson::value(std::move(obj)).serialize());
  });

  pc->onGatheringStateChange([](rtc::PeerConnection::GatheringState state) {
    if (state == rtc::PeerConnection::GatheringState::Complete)
      INFO_LOG_FMT(COMMON, "AVStreamPeer: ICE gathering complete");
  });

  const auto control_channel = pc->createDataChannel("control");

  control_channel->onOpen([] { INFO_LOG_FMT(COMMON, "AVStreamPeer: Data channel opened"); });

  // FYI: `control_channel` is captured to keep it from going out of scope until closure.
  control_channel->onMessage(
      [control_channel](const rtc::binary& message) {
        WARN_LOG_FMT(COMMON,
                     "AVStreamPeer: Received unexpected binary data (size: {}) on control channel",
                     message.size());
      },
      [this](const std::string& message) {
        DEBUG_LOG_FMT(COMMON, "AVStreamPeer: Received: {}", message);

        picojson::value json;
        const auto err = picojson::parse(json, message);

        if (!err.empty())
          return;

        if (!json.is<picojson::object>())
          return;

        m_control_callback(json.get<picojson::object>());
      });

  // FYI: Arbitrary values. We're just following the libdatachannel examples.
  constexpr u8 VIDEO_PT = 102;
  constexpr u8 AUDIO_PT = 111;
  constexpr u32 VIDEO_SSRC = 1;
  constexpr u32 AUDIO_SSRC = 2;

  rtc::Description::Video video;
  video.addH264Codec(VIDEO_PT);
  video.addSSRC(VIDEO_SSRC, "video-stream", "stream", "video0");

  const auto video_config = std::make_shared<rtc::RtpPacketizationConfig>(
      VIDEO_SSRC, "video-stream", VIDEO_PT, rtc::RtpPacketizer::VideoClockRate);
  auto video_packetizer = std::make_shared<rtc::H264RtpPacketizer>(
      rtc::NalUnit::Separator::StartSequence, video_config);
  video_packetizer->addToChain(std::make_shared<rtc::RtcpSrReporter>(video_config));
  video_packetizer->addToChain(std::make_shared<rtc::RtcpNackResponder>());

  const auto video_track = pc->addTrack(video);
  video_track->setMediaHandler(std::move(video_packetizer));

  rtc::Description::Audio audio;
  audio.addOpusCodec(AUDIO_PT);
  audio.addSSRC(AUDIO_SSRC, "audio-stream", "stream", "audio0");

  const auto audio_config = std::make_shared<rtc::RtpPacketizationConfig>(
      AUDIO_SSRC, "audio-stream", AUDIO_PT, rtc::OpusRtpPacketizer::DefaultClockRate);
  auto audio_packetizer = std::make_shared<rtc::OpusRtpPacketizer>(audio_config);
  audio_packetizer->addToChain(std::make_shared<rtc::RtcpSrReporter>(audio_config));
  audio_packetizer->addToChain(std::make_shared<rtc::RtcpNackResponder>());

  const auto audio_track = pc->addTrack(audio);
  audio_track->setMediaHandler(std::move(audio_packetizer));

  // FYI: The track handlers hold a shared_ptr to the tracks themselves.
  // Track closure removes all handlers, allowing the track to go out of scope.
  video_track->onOpen([&, video_track] { m_av_stream_impl->m_video_tracks.Append(video_track); });
  audio_track->onOpen([&, audio_track] { m_av_stream_impl->m_audio_tracks.Append(audio_track); });
}

bool AVStreamPeer::HandleMessage(const std::string& message)
// FYI: rtc::PeerConnection member functions can throw.
// Since that has to be dealt with using a try block, regardless,
//  exceptions are used throughout this function for errors.
try
{
  // TODO: Remove this hack. It's working around a libdatachannel bug.
  // https://github.com/paullouisageneau/libdatachannel/issues/1647
  std::locale::global(std::locale::classic());

  picojson::value json;
  const std::string err = picojson::parse(json, message);

  if (!err.empty())
    throw std::invalid_argument{fmt::format("JSON: {}", err)};

  if (!json.is<picojson::object>())
    throw std::invalid_argument{"JSON: Not an object"};

  const auto& obj = json.get<picojson::object>();

  const auto get_field = [&](const char* name) {
    if (const auto str = ReadStringFromJson(obj, name))
      return *str;

    throw std::logic_error(fmt::format("JSON: Missing field: {}", name));
  };

  const std::string type = get_field("type");

  if (type == "request")
  {
    if (m_impl->m_peer_connection)
      throw std::logic_error{"Unexpected request message"};

    m_impl->CreatePeerConnection();
    m_impl->m_peer_connection->setLocalDescription(rtc::Description::Type::Offer);

    return true;
  }

  if (!m_impl->m_peer_connection)
    throw std::logic_error{"Expected request message"};

  if (type == "answer")
  {
    const auto sdp = get_field("description");
    m_impl->m_peer_connection->setRemoteDescription({sdp, rtc::Description::Type::Answer});
  }
  else if (type == "candidate")
  {
    auto candidate = get_field("candidate");
    auto mid = get_field("mid");
    m_impl->m_peer_connection->addRemoteCandidate({std::move(candidate), std::move(mid)});
  }
  else
  {
    throw std::logic_error{fmt::format("Unexpected message type: {}", type)};
  }

  return true;
}
catch (const std::exception& e)
{
  ERROR_LOG_FMT(COMMON, "HandleMessage: {}", e.what());
  return false;
}

void AVStreamPeer::SetSendMessageCallback(SendCallbackType send_callback)
{
  m_impl->m_send_callback = std::move(send_callback);
}

void AVStreamPeer::SetControlMessageCallback(ControlCallbackType callback)
{
  m_impl->m_control_callback = std::move(callback);
}

AVStream::AVStream() : m_impl{std::make_shared<Impl>()}
{
  INFO_LOG_FMT(COMMON, "AVStream: Constructed");

  m_audio_encoder = CreateOpusEncoder();
  m_audio_encoder->SetPacketCallback(std::bind_front(&Impl::SendAudioDataToAllPeers, m_impl.get()));

  m_video_encoder = CreateH264Encoder();
  m_video_encoder->SetPacketCallback(std::bind_front(&Impl::SendVideoDataToAllPeers, m_impl.get()));
}

AVStream::~AVStream()
{
  INFO_LOG_FMT(COMMON, "AVStream: Stopped");
}

void AVStream::TakeAudioSamples(std::size_t frame_count,
                                Common::MoveOnlyFunction<void(std::span<u8>)> fill_buffer_callback)
{
  const auto start_time = Clock::now();

  m_audio_encoder->TakeAudioSamples(frame_count, std::move(fill_buffer_callback));

  const auto stall_time = Clock::now() - start_time;
  if (stall_time > std::chrono::milliseconds{1})
    WARN_LOG_FMT(COMMON, "OpusEncoder stalled us for {:.2f} ms", DT_ms{stall_time}.count());
}

void AVStream::PushVideoFrame(VideoEncoder::FrameDetails frame)
{
  const auto start_time = Clock::now();

  m_video_encoder->PushVideoFrame(frame);

  const auto stall_time = Clock::now() - start_time;
  if (stall_time > std::chrono::milliseconds{1})
    WARN_LOG_FMT(COMMON, "H264Encoder stalled us for {:.2f} ms", DT_ms{stall_time}.count());
}

void AVStream::FlushVideo()
{
  m_video_encoder->Flush();
}

void AVStream::Impl::SendAudioDataToAllPeers(std::span<const u8> data, s64 pts)
{
  // Our sample rate is the same as the clock rate.
  static_assert(rtc::OpusRtpPacketizer::DefaultClockRate == AUDIO_SAMPLE_RATE);
  const rtc::FrameInfo frame_info{u32(pts)};

  m_audio_tracks.ForEach([&](rtc::Track& track) {
    try
    {
      // FYI: sendFrame throws if the track is closed in the meanwhile. It's unavoidable.
      track.sendFrame(reinterpret_cast<const std::byte*>(data.data()), data.size(), frame_info);
    }
    catch (const std::runtime_error& err)
    {
      (void)err;
    }
  });
}

void AVStream::Impl::SendVideoDataToAllPeers(std::span<const u8> data, s64 pts)
{
  using VideoClockDuration =
      std::chrono::duration<u32, std::ratio<1, rtc::RtpPacketizer::VideoClockRate>>;

  // FYI: Ignoring pts and using a real timestamp handles fast forwarding much better,
  //  and it also avoids worrying about the source framerate in general.
  (void)pts;
  const rtc::FrameInfo frame_info{
      duration_cast<VideoClockDuration>(Clock::now().time_since_epoch()).count()};

  m_video_tracks.ForEach([&](rtc::Track& track) {
    try
    {
      // FYI: sendFrame throws if the track is closed in the meanwhile. It's unavoidable.
      track.sendFrame(reinterpret_cast<const std::byte*>(data.data()), data.size(), frame_info);
    }
    catch (const std::runtime_error& err)
    {
      (void)err;
    }
  });
}

}  // namespace WebUI

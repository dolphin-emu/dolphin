// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/WebUI/AVStreamRTCPeer.h"

#include <rtc/rtc.hpp>

#include <picojson.h>

#include "Common/Config/Config.h"
#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/WeakInstance.h"

#include "Core/Config/MainSettings.h"

#include "UICommon/WebUI/AVStream.h"

namespace
{

// This hack works around a recently fixed LibDataChannel bug.
// https://github.com/paullouisageneau/libdatachannel/issues/1647
// We must temporarily adjust the global locale to prevent bad serialization.
[[nodiscard]] std::shared_ptr<void> GetScopedClassicLocaleHack()
{
  // There's no way to check the runtime library version so let's test if it's broken.
  // We only run the test once. This assumes we continue using the same locale from startup.
  static const bool s_is_hack_needed = [] {
    // Bugged versions will erroneously add thousand separators based on the locale.
    static constexpr auto candidate_test_str =
        "candidate:1 1 UDP 2114977791 127.0.0.1 58302 typ host";
    if (rtc::Candidate{candidate_test_str}.candidate() == candidate_test_str)
      return false;

    WARN_LOG_FMT(COMMON, "LibDataChannel has broken serialization. The global locale will be "
                         "sporadically adjusted to compensate.");
    return true;
  }();

  if (!s_is_hack_needed)
    return nullptr;

  struct ScopedClassicLocale
  {
    const std::locale previous_locale{std::locale::global(std::locale::classic())};
    ~ScopedClassicLocale() { std::locale::global(previous_locale); }
  };

  // LibDataChannel is multithreaded. We allow for overlapping usages by making one shared object.
  static Common::WeakInstance<ScopedClassicLocale> s_instance;
  return s_instance.GetOrCreate();
}

constexpr u32 AUDIO_CLOCK_RATE = rtc::OpusRtpPacketizer::DefaultClockRate;
constexpr u32 VIDEO_CLOCK_RATE = rtc::RtpPacketizer::VideoClockRate;

struct AudioTrackFrameSender
{
  void operator()(std::span<const u8> data, s64 pts)
  try
  {
    // Our sample rate is the same as the clock rate.
    static_assert(AUDIO_CLOCK_RATE == WebUI::AUDIO_SAMPLE_RATE);
    const rtc::FrameInfo frame_info{u32(pts)};

    // FYI: sendFrame throws if the track is closed in the meanwhile. It's unavoidable.
    track->sendFrame(reinterpret_cast<const std::byte*>(data.data()), data.size(), frame_info);
  }
  catch (const std::runtime_error& err)
  {
    (void)err;
  }

  std::shared_ptr<rtc::Track> track;
};

struct VideoTrackFrameSender
{
  void operator()(std::span<const u8> data, s64 pts)
  try
  {
    using VideoClockDuration = std::chrono::duration<u32, std::ratio<1, VIDEO_CLOCK_RATE>>;

    // FYI: Ignoring pts and using a real timestamp handles fast forwarding much better,
    //  and it also avoids worrying about the source framerate in general.
    (void)pts;
    const rtc::FrameInfo frame_info{
        duration_cast<VideoClockDuration>(Clock::now().time_since_epoch()).count()};

    // FYI: sendFrame throws if the track is closed in the meanwhile. It's unavoidable.
    track->sendFrame(reinterpret_cast<const std::byte*>(data.data()), data.size(), frame_info);
  }
  catch (const std::runtime_error& err)
  {
    (void)err;
  }

  std::shared_ptr<rtc::Track> track;
};

}  // namespace

namespace WebUI
{

struct AVStreamRTCPeer::Impl
{
  void CreatePeerConnection();

  void SendMessage(const std::string& message) { m_send_callback(message); }

  std::shared_ptr<AVStream> m_av_stream;

  std::shared_ptr<rtc::PeerConnection> m_peer_connection;

  AVStreamRTCPeer::SendCallbackType m_send_callback;
  AVStreamRTCPeer::ControlCallbackType m_control_callback;
};

AVStreamRTCPeer::AVStreamRTCPeer(std::shared_ptr<AVStream> av_stream)
    : m_impl{std::make_unique<Impl>(av_stream)}
{
}

AVStreamRTCPeer::~AVStreamRTCPeer() = default;

void AVStreamRTCPeer::Impl::CreatePeerConnection()
{
  rtc::Configuration config{
      // We manually create the offer after our channels to avoid unnecessary renegotiation.
      .disableAutoNegotiation = true,
  };

  for (const auto& ice_server :
       SplitString(Config::Get(Config::MAIN_WEB_INTERFACE_ICE_SERVERS), ' '))
  {
    INFO_LOG_FMT(COMMON, "CreatePeerConnection: Adding ICE server: {}", ice_server);
    config.iceServers.emplace_back(ice_server);
  }

  m_peer_connection = std::make_shared<rtc::PeerConnection>(config);
  auto& pc = *m_peer_connection;

  pc.onLocalDescription([this](const rtc::Description& description) {
    // This hack works around a recently fixed LibDataChannel bug.
    auto hack_locale = GetScopedClassicLocaleHack();

    picojson::object obj;
    obj.emplace("type", description.typeString());
    obj.emplace("sdp", description.generateSdp());
    obj.emplace("iceServers", Config::Get(Config::MAIN_WEB_INTERFACE_ICE_SERVERS));
    SendMessage(picojson::value(std::move(obj)).serialize());
  });

  pc.onLocalCandidate([this](const rtc::Candidate& candidate) {
    // This hack works around a recently fixed LibDataChannel bug.
    auto hack_locale = GetScopedClassicLocaleHack();

    picojson::object obj;
    obj.emplace("type", "candidate");
    obj.emplace("candidate", candidate.candidate());
    obj.emplace("mid", candidate.mid());
    SendMessage(picojson::value(std::move(obj)).serialize());
  });

  const auto control_channel = pc.createDataChannel("control");

  control_channel->onOpen([] { INFO_LOG_FMT(COMMON, "Data channel opened"); });

  // FYI: `control_channel` is captured to keep it from going out of scope until closure.
  control_channel->onMessage(
      [control_channel](const rtc::binary& message) {
        WARN_LOG_FMT(COMMON, "Unexpected binary data (size: {})", message.size());
      },
      [this](const std::string& message) {
        DEBUG_LOG_FMT(COMMON, "Received: {}", message);

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
      VIDEO_SSRC, "video-stream", VIDEO_PT, VIDEO_CLOCK_RATE);
  auto video_packetizer = std::make_shared<rtc::H264RtpPacketizer>(
      rtc::NalUnit::Separator::StartSequence, video_config);
  video_packetizer->addToChain(std::make_shared<rtc::RtcpSrReporter>(video_config));
  video_packetizer->addToChain(std::make_shared<rtc::RtcpNackResponder>());

  const auto video_track = pc.addTrack(video);
  video_track->setMediaHandler(std::move(video_packetizer));

  rtc::Description::Audio audio;
  audio.addOpusCodec(AUDIO_PT);
  audio.addSSRC(AUDIO_SSRC, "audio-stream", "stream", "audio0");

  const auto audio_config = std::make_shared<rtc::RtpPacketizationConfig>(
      AUDIO_SSRC, "audio-stream", AUDIO_PT, AUDIO_CLOCK_RATE);
  auto audio_packetizer = std::make_shared<rtc::OpusRtpPacketizer>(audio_config);
  audio_packetizer->addToChain(std::make_shared<rtc::RtcpSrReporter>(audio_config));
  audio_packetizer->addToChain(std::make_shared<rtc::RtcpNackResponder>());

  const auto audio_track = pc.addTrack(audio);
  audio_track->setMediaHandler(std::move(audio_packetizer));

  // FYI: The track handlers hold a shared_ptr to the tracks themselves.
  // Track close removes all handlers, allowing the track to go out of scope.
  auto video_track_handler =
      std::make_shared<AVStream::PacketHandler>(VideoTrackFrameSender{video_track});
  auto audio_track_handler =
      std::make_shared<AVStream::PacketHandler>(AudioTrackFrameSender{audio_track});

  video_track->onOpen(
      [this, video_track_handler] { m_av_stream->AddVideoPacketHandler(video_track_handler); });
  audio_track->onOpen(
      [this, audio_track_handler] { m_av_stream->AddAudioPacketHandler(audio_track_handler); });

  // This hack works around a recently fixed LibDataChannel bug.
  auto hack_locale = GetScopedClassicLocaleHack();

  // This triggers onLocalDescription, which we then forward to the client.
  pc.setLocalDescription(rtc::Description::Type::Offer);
}

bool AVStreamRTCPeer::HandleMessage(const std::string& message)
// FYI: rtc::PeerConnection member functions can throw.
// Since that has to be dealt with using a try block, regardless,
//  exceptions are used throughout this function for errors.
try
{
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

  // Client -> type:request
  // Server -> type:offer (with SDP) (and a list of ICE servers to use)
  // Client -> type:answer (with SDP)
  // type:candidate messages are sent in both directions after the request.

  if (type == "request")
  {
    if (m_impl->m_peer_connection)
      throw std::logic_error{"Unexpected request message"};

    m_impl->CreatePeerConnection();

    return true;
  }

  if (!m_impl->m_peer_connection)
    throw std::logic_error{"Expected request message"};

  if (type == "answer")
  {
    // This hack works around a recently fixed LibDataChannel bug.
    auto hack_locale = GetScopedClassicLocaleHack();

    m_impl->m_peer_connection->setRemoteDescription({
        get_field("sdp"),
        rtc::Description::Type::Answer,
    });
  }
  else if (type == "candidate")
  {
    // This hack works around a recently fixed LibDataChannel bug.
    auto hack_locale = GetScopedClassicLocaleHack();

    m_impl->m_peer_connection->addRemoteCandidate({
        get_field("candidate"),
        get_field("mid"),
    });
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

void AVStreamRTCPeer::SetSendMessageCallback(SendCallbackType send_callback)
{
  m_impl->m_send_callback = std::move(send_callback);
}

void AVStreamRTCPeer::SetControlMessageCallback(ControlCallbackType callback)
{
  m_impl->m_control_callback = std::move(callback);
}

}  // namespace WebUI

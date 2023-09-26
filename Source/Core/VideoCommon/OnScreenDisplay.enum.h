#pragma once
namespace OSD
{
enum class MessageStackDirection
{
  Downward = 1,
  Upward = 2,
  Rightward = 4,
  Leftward = 8,
};
enum class MessageType
{
  NetPlayPing,
  NetPlayBuffer,

  // This entry must be kept last so that persistent typed messages are
  // displayed before other messages
  Typeless,
};
}

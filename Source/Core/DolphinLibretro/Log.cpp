
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "DolphinLibretro/Log.h"
#include "DolphinLibretro/Options.h"

namespace Libretro
{
extern retro_environment_t environ_cb;
namespace Log
{
class LogListener : public ::LogListener
{
public:
  LogListener(retro_log_printf_t log);
  ~LogListener() override;
  void Log(LogTypes::LOG_LEVELS level, const char* text) override;

private:
  retro_log_printf_t m_log;
};

static LogListener* logListener;

void Init()
{
  struct retro_log_callback log = {};
  if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log) && log.log)
    logListener = new LogListener(log.log);
}

void Shutdown()
{
  delete logListener;
  logListener = nullptr;
}

LogListener::LogListener(retro_log_printf_t log) : m_log(log)
{
  LogManager::GetInstance()->SetLogLevel(Libretro::Options::logLevel);
  LogManager::GetInstance()->SetEnable(LogTypes::BOOT, true);
  LogManager::GetInstance()->SetEnable(LogTypes::CORE, true);
  LogManager::GetInstance()->SetEnable(LogTypes::VIDEO, true);
  LogManager::GetInstance()->SetEnable(LogTypes::HOST_GPU, true);
  LogManager::GetInstance()->SetEnable(LogTypes::COMMON, true);
  LogManager::GetInstance()->SetEnable(LogTypes::MEMMAP, true);
  LogManager::GetInstance()->SetEnable(LogTypes::DSPINTERFACE, true);
  LogManager::GetInstance()->SetEnable(LogTypes::DSPHLE, true);
  LogManager::GetInstance()->SetEnable(LogTypes::DSPLLE, true);
  LogManager::GetInstance()->SetEnable(LogTypes::DSP_MAIL, true);
  LogManager::GetInstance()->RegisterListener(LogListener::CUSTOM_LISTENER, this);
  LogManager::GetInstance()->EnableListener(LogListener::CONSOLE_LISTENER, false);
  LogManager::GetInstance()->EnableListener(LogListener::CUSTOM_LISTENER, true);
}

LogListener::~LogListener()
{
  LogManager::GetInstance()->EnableListener(LogListener::CUSTOM_LISTENER, false);
  LogManager::GetInstance()->EnableListener(LogListener::CONSOLE_LISTENER, true);
  LogManager::GetInstance()->RegisterListener(LogListener::CONSOLE_LISTENER, nullptr);
}

void LogListener::Log(LogTypes::LOG_LEVELS level, const char* text)
{
  switch (level)
  {
  case LogTypes::LOG_LEVELS::LDEBUG:
    m_log(RETRO_LOG_DEBUG, text);
    break;
  case LogTypes::LOG_LEVELS::LWARNING:
    m_log(RETRO_LOG_WARN, text);
    break;
  case LogTypes::LOG_LEVELS::LERROR:
    m_log(RETRO_LOG_ERROR, text);
    break;
  case LogTypes::LOG_LEVELS::LNOTICE:
  case LogTypes::LOG_LEVELS::LINFO:
  default:
    m_log(RETRO_LOG_INFO, text);
    break;
  }
}
}  // namespace Log
}  // namespace Libretro

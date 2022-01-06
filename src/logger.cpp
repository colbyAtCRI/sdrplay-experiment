#include "logger.h"
#include "utility.h"

// Start with a fixed name. We can rename it later in software
std::ofstream Logger::mLog("run.log");

// All log entries start with the system time.
std::ofstream &Logger::log()
{
  mLog << systemTime();
  return mLog;
}

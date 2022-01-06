#pragma once
#include <iostream>
#include <fstream>

// The idea here is to use inheritance to add info to the log entry.
// Each class that can make log entries will add it's info to the
// entry by providing a log() member which calls the parent class
// log(). 
class Logger
{
  // There is one log for the run
  static std::ofstream mLog;
public:
  Logger() {}
  std::ofstream &log();
};

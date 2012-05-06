#ifndef __ctc_logger_h__
#define __ctc_logger_h__

#include <string>
#include <fstream>
#include <iostream>

class Logger
{
public:
    static Logger *instance();
    static std::ostream *logs;
    static std::ofstream *logfs;
    int openLogFile(const std::string &log_file);
    void closeLogFile();

private:
    Logger();
    Logger(const Logger &) {}
    Logger &operator=(const Logger &) {}

    static Logger *_instance;
};

#define LOG_MSG (*(Logger::logs)) << "[" << getCurrentTime() << "]\t"
#define DBG_LOG if (SysConfig::instance()->getDebug()) LOG_MSG

#define VLOG if (SysConfig::instance()->getVERBOSE()) cout  << "[" << getCurrentTime() << "]\t"

#endif

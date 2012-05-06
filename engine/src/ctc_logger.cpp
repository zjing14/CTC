#include "ctc_logger.h"
#include <iostream>
using namespace std;

Logger *Logger::_instance = NULL;
ostream *Logger::logs = NULL;
ofstream *Logger::logfs = NULL;

Logger::Logger()
{
    logs = &cout;
}

Logger *Logger::instance()
{
    if(_instance == NULL) {
        _instance = new Logger();
    }

    return _instance;
}

int Logger::openLogFile(const string &log_file)
{
    logfs  = new ofstream(log_file.c_str(), ios::out | ios::app);

    if(!(logfs->is_open())) {
        throw __FILE__ " Logger::openLogFile: Error opening the log file";
    }

    logs = logfs;

    return 0;
}

void Logger::closeLogFile()
{
    logfs->close();
}

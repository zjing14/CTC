#ifndef __ctc_sysconfig_h__
#define __ctc_sysconfig_h__

#include <string>

class SysConfig
{
public:
    static SysConfig *instance();
    int openConfigFile(const std::string &conf_file);

    std::string getLogFileName() {
        return _log_file;
    }
    std::string getWorkDir() {
        return _work_dir;
    }
    std::string getPluginDir() {
        return _plugin_dir;
    }
    std::string getWorkflowDir() {
        return _workflow_dir;
    }
    bool getVerbose() {
        return _verbose;
    }
    bool getDebug() {
        return _debug;
    }

    void setVerbose(bool flag) {
        _verbose = flag;
    }
    void setDebug(bool flag) {
        _debug = flag;
    }
    void setLogFile(const std::string &file) {
        _log_file = file;
    }
    void setWorkDir(const std::string &dir) {
        _work_dir = dir;
    }
    void setPluginDir(const std::string &dir) {
        _plugin_dir = dir;
    }
    void setWorkflowDir(const std::string &dir) {
        _workflow_dir = dir;
    }

private:
    SysConfig();
    SysConfig(const SysConfig &) {}
    SysConfig &operator=(const SysConfig &) {}

    std::string _log_file;
    std::string _work_dir;
    std::string _plugin_dir;
    std::string _workflow_dir;
    bool _verbose;
    bool _debug;

    static SysConfig *_instance;
};

#define DEBUG SysConfig::instance()->getDebug()
#define VERBOSE SysConfig::instance()->getVerbose()

#endif

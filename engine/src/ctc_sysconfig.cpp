#include "ctc_sysconfig.h"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <cctype>

using namespace std;

SysConfig *SysConfig::_instance = NULL;

SysConfig *SysConfig::instance()
{
    if(_instance == NULL) {
        _instance = new SysConfig();
    }
    return _instance;
}

SysConfig::SysConfig()
{
    _log_file = "";
    _work_dir = "";
    _verbose = false;
    _debug = false;
}

int SysConfig::openConfigFile(const string &conf_file)
{
    ifstream ifs;
    ifs.open(conf_file.c_str());

    if(!ifs.is_open()) {
        ofstream ofs(conf_file.c_str());
        if(!ofs.is_open()) {
            cerr << "Opening configuration file: " << conf_file << endl;
            throw __FILE__ " SysConfig::openConfigFile: Cannot open configuration file for write";
        }
        // Default configuration
        ofs << "verbose = yes" << endl;
        ofs << "debug = yes" << endl;
        ofs << "work_dir = workspace" << endl;
        ofs << "plugin_dir = plugins" << endl;
        ofs << "workflow_dir = workflows" << endl;
        ofs << "log_file = ctc.log" << endl;

        ofs.close();

        ifs.open(conf_file.c_str());
        if(!ifs.is_open()) {
            throw __FILE__ " SysConfig::openConfigFile: Cannot open configuration file";
        }
    }

    while(!ifs.eof()) {
        string line;
        getline(ifs, line);

        if(line[0] == ';' || line.size() == 0) { // comment
            continue;
        }

        size_t delim = line.find("=");
        if(delim == string::npos) {
            cerr << "WARNING: Invalid config line in " << conf_file << endl;
            continue;
        }

        string key = line.substr(0, delim);
        key.erase(remove_if(key.begin(), key.end(), (int( *)(int))isspace), key.end());
        string val = line.substr(delim + 1);
        val.erase(remove_if(val.begin(), val.end(), (int( *)(int))isspace), val.end());

        if(key == "log_file") {
            _log_file = val;
        } else if(key == "work_dir") {
            _work_dir = val;
        } else if(key == "plugin_dir") {
            _plugin_dir = val;
        } else if(key == "workflow_dir") {
            _workflow_dir = val;
        } else if(key == "verbose") {
            if(val == "yes") {
                _verbose = true;
            } else {
                _verbose = false;
            }
        } else if(key == "debug") {
            if(val == "yes") {
                _debug = true;
            } else {
                _debug = false;
            }
        }
    }

    ifs.close();

    return 0;
}

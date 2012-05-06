#include "ctc_engine.h"
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>

using namespace std;

int main(int argc, char **argv)
{

    if(argc < 3) {
        cerr << "Usage: " << argv[0] << " <engine_root_dir> <plugin|workflow> <id> [--test] [--verbose] [--debug [log_file]] [--workdir <dir>] parameters" << endl;
        exit(-1);
    }

    int config_opt, arg_idx = 0, opt_idx = 0;
    opterr = 0;

    struct option long_opts[] = {
        {"test", no_argument, &config_opt, 1 },
        {"verbose", no_argument, &config_opt, 2},
        {"debug", optional_argument, &config_opt, 3},
        {0, 0, 0, 0}
    };

    try {
        string engine_dir = argv[++arg_idx];
        SysConfig *sys_conf = SysConfig::instance();
        sys_conf->openConfigFile(engine_dir + "/" + "ctc.ini");

        bool dry_run = false;
        int ac = 0;
        char **av = &(argv[arg_idx]);

        for(; arg_idx < argc; arg_idx++, ac++) {
            if(string(argv[arg_idx]) == "plugin" || string(argv[arg_idx]) == "workflow") {
                break;
            }
        }

        string exec_cmd = argv[arg_idx];
        string id = argv[++arg_idx];

        int opt_code;
        while((opt_code = getopt_long(ac, av, "", long_opts, &opt_idx)) != -1) {
            switch(opt_code) {
            case 0:
                switch(config_opt) {
                case 1:
                    dry_run = true;
                    break;
                case 2:
                    sys_conf->setVerbose(true);
                    break;
                case 3:
                    sys_conf->setDebug(true);
                    if(optarg != NULL) {
                        sys_conf->setLogFile(optarg);
                    } else {
                        sys_conf->setLogFile("");
                    }
                    break;
                default:
                    throw __FILE__ " Unrecognized engine parameters";
                    break;
                }
                break;
            default:
                throw __FILE__ " Unrecognized engine parameters";
            }
        }

        Engine engine(engine_dir, sys_conf);
        map <string, string> paras;

        arg_idx++;
        // Parameters should be given as: --paraname1 value1 --paraname2 value2 ...
        for(; arg_idx < argc; arg_idx += 2) {
            string para_name = argv[arg_idx];
            if((para_name.length() < 3 || para_name.substr(0, 2) != "--") || (arg_idx == argc - 1)) {
                cerr << "Error parsing parameters!" << endl;
                exit(-1);
            }
            paras[para_name.substr(2)] = argv[arg_idx + 1];
        }

        if(exec_cmd == "plugin") {
            engine.executePlugin(id, paras, dry_run);
        } else if(exec_cmd == "workflow") {
            engine.executeWorkflow(id, paras, dry_run);
        } else {
            throw " invalid execution commnd";
        }
    } catch(const char *error) {
        cerr << error << endl;
        exit(-1);
    }

    return 0;
}

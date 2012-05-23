#include "ctc_engine.h"
#include "ctc_workflow.h"
#include "ctc_util.h"
#include "ctc_plugin.h"
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
using namespace std;

int main(int argc, char **argv) {
    if(argc != 2) {
        cerr << "Usage: " << argv[0] << " <engine_root>" << endl;
        return -1;
    }

    string root = argv[1];
    Engine engine(root);

    string work_dir = SysConfig::instance()->getWorkDir();
    if(work_dir.c_str()[work_dir.length() - 1] != '/')
        work_dir += '/';
    string command = "rm -rf " + work_dir + '*';
    return system(command.c_str());
}

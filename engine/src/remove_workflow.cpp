#include "ctc_engine.h"
#include "ctc_workflow.h"
#include "ctc_util.h"
#include "ctc_plugin.h"
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
using namespace std;

int main(int argc, char **argv)
{
    if(argc != 3) {
        cerr << "Usage: " << argv[0] << " <engine_root> <workflow_id>" << endl;
        return -1;
    }

    string root = argv[1];
    string id = argv[2];
    Engine engine(root);

    Catalog *catalog = Catalog::instance();

    try {
        catalog->removeWorkflow(id);
        catalog->updateConfigFile();
    } catch (const char* err) {
        cerr << err << endl;
    }

    return 0;
}

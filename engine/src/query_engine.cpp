#include "ctc_engine.h"
#include <iostream>
#include <stdlib.h>
using namespace std;

int main(int argc, char **argv)
{

    if(argc < 3) {
        cerr << "Usage: " << argv[0] << " <engine_root_dir> listPlugins|listWorkflows|listSuites|queryPlugin|queryWorflow [id]" << endl;
        exit(-1);
    }

    try {
        Engine engine(argv[1]);

        string query_cmd = argv[2];

        if(query_cmd == "listPlugins") {
            engine.listPlugins();
        } else if(query_cmd == "listWorkflows") {
            engine.listWorkflows();
        } else if(query_cmd == "listSuites") {
            engine.listSuites();
        } else if(query_cmd == "queryPlugin") {
            if(argc < 4) {
                throw " missing Plugin ID";
            }
            engine.queryPlugin(argv[3]);
        } else if(query_cmd == "queryWorkflow") {
            if(argc < 4) {
                throw " missing Workflow ID";
            }
            engine.queryWorkflow(argv[3]);
        } else {
            throw " invalid query commnd";
        }
    } catch(const char *error) {
        cerr << error << endl;
        exit(-1);
    }

    return 0;
}

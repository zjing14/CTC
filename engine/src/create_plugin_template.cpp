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
        cerr << "Usage: " << argv[0] << " <engine_root> <plugin_id>" << endl;
        return -1;
    }

    Engine engine(argv[1]);
    string id = argv[2];

    Catalog *catalog = Catalog::instance();
    string suite_id, name, version, description;

    try {
        if(catalog->hasPlugin(id)) {
            throw __FILE__ " Plugin with the same id exists";
        }

        cout << "Suite: ";
        getline(cin, suite_id);
        cout << "Name: ";
        getline(cin, name);
        cout << "Version: ";
        getline(cin, version);
        cout << "Description: ";
        getline(cin, description);

        Plugin plugin;
        plugin.setID(id);
        plugin.setName(name);
        plugin.setVersion(version);
        plugin.setDesc(description);

        // Add one input parameter and output parameter
        Parameter p(DATA_FILE, "", "", "");
        plugin.addInput("input", p);
        plugin.addOutput("output", p);
        plugin.addCommand("add command here");

        string conf_file = SysConfig::instance()->getPluginDir() + "/" + id + ".xml";
        if(catalog->addPlugin(suite_id, id, conf_file)) {
            throw __FILE__ " Error adding plugin to the catalog";
        }

        plugin.buildXml(conf_file);
        catalog->updateConfigFile();

        cout << "A template of the plugin has been placed in " << conf_file << endl;
    } catch(char *err_msg) {
        cerr << err_msg << endl;
    }

    return 0;
}

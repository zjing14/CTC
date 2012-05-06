#include "ctc_catalog.h"
#include "ctc_plugin.h"
#include "ctc_workflow.h"
#include "ctc_engine.h"
#include "ctc_sysconfig.h"
#include "ctc_logger.h"
#include <iostream>
#include <fstream>

using namespace std;

void processCommands(Plugin *pp, vector <string>& strings);

int main(int argc, char **argv)
{
    if(argc != 3) {
        cerr << "Usage: " << argv[0] << " <engine_root> <script>" << endl;
        return(-1);
    }

    string engine_root = argv[1];
    string script_file = argv[2];

    Workflow *wp = new Workflow();
    vector <Plugin *> plugins;
    vector <WorkflowStep *> steps;
    string suite;
    string last_output = "$input";

    try {
        ifstream ifs(script_file.c_str());
        if(!ifs.is_open()) {
            throw __FILE__ " Error opening the script";
        }

        Engine p(engine_root);

        Parameter p1(DATA_FILE, "", "", "");
        wp->addInput("input", p1);
        int step_idx = 0;

        string line;
        while(ifs.good()) {
            getline(ifs, line);

            size_t pos;
            if(line.c_str()[0] == '#') { // comment line
                continue;
            }
            if((pos = line.find(":=")) != string::npos) { // plugin definition
                if(line.find("{", pos) == string::npos) {
                    throw __FILE__ " Error parsing a plugin definition";
                }

                Plugin *pp = new Plugin();
                string plugin_id = trimString(line.substr(0, pos));
                pp->setID(plugin_id);
                Parameter p2(DATA_FILE, "", "", "");
                pp->addInput("input", p2);
                Parameter p3(DATA_FILE, "", "", "");
                pp->addOutput("output", p3);

                vector <string> cmd_strings;
                bool matched = false;
                bool has_output = false;
                while(ifs.good()) {
                    getline(ifs, line);
                    if(trimString(line) == "}") {
                        matched = true;
                        break;
                    } else {
                        if(line.find("$output") != string::npos || line.find("${output}") != string::npos) {
                            has_output = true;
                        }
                        cmd_strings.push_back(line);
                    }
                }
                if(matched) {
                    processCommands(pp, cmd_strings);
                } else {
                    throw __FILE__ "Incomplete plugin definition";
                }

                plugins.push_back(pp);

                WorkflowStep *wsp = new WorkflowStep(PLUGIN, plugin_id, plugin_id);
                wsp->addInput("input", last_output);
                wsp->addOutput("output");
                steps.push_back(wsp);

                wp->addStep(*wsp);
                step_idx++;

                // Update last_output
                if(has_output) {
                    last_output = "$" + plugin_id + ".output";
                }
            } else if((pos = line.find("=")) != string::npos) { // This is a variable definition
                string name = trimString(line.substr(0, pos));
                string value = trimString(line.substr(pos + 1));
                // Remove quote
                if(value.c_str()[0] == '"') {
                    value.erase(0, 1);
                }
                if(value.c_str()[value.length() - 1] == '"') {
                    value.erase(value.length() - 1, 1);
                }
                if(name == "WORKFLOW_ID") {
                    wp->setID(value);
                } else if(name == "WORKFLOW_NAME") {
                    wp->setName(value);
                } else if(name == "WORKFLOW_VERSION") {
                    wp->setVersion(value);
                } else if(name == "SUITE_ID") {
                    suite = value;
                } else {
                    wp->addVar(name, value);
                }
            } // skip other lines
        }
        ifs.close();

        Parameter p4(DATA_FILE, last_output, "", "");
        wp->addOutput("output", p4);

        // Save plugins and workflow
        Catalog *cp = Catalog::instance();

        vector <string> plugin_files;
        for(int i = 0; i < plugins.size(); i++) {
            string p_conf = SysConfig::instance()->getPluginDir() + "/" + plugins[i]->getID() + ".xml";
            if(cp->addPlugin(suite, plugins[i]->getID(), p_conf)) {
                throw __FILE__ " Error adding plugin to the catalog";
            }
            plugin_files.push_back(p_conf);
        }
        string w_conf = SysConfig::instance()->getWorkflowDir() + "/" + wp->getID() + ".xml";
        if(cp->addWorkflow(suite, wp->getID(), w_conf)) {
            throw __FILE__ " Error adding workflow to the catalog";
        }

        for(int i = 0; i < plugins.size(); i++) {
            DBG_LOG << "Writing to: " << plugin_files[i] << endl;
            plugins[i]->buildXml(plugin_files[i]);
        }
        DBG_LOG << "Writing to: " << w_conf << endl;
        wp->buildXml(w_conf);

        // Update catalog.xml
        cp->updateConfigFile();

    } catch(const char *err_msg) {
        cerr << err_msg << endl;
    }

    // Cleanup
    delete wp;
    for(int i = 0; i < plugins.size(); i++) {
        delete plugins[i];
        delete steps[i];
    }

    return 0;
}

void processCommands(Plugin *pp, vector <string>& strings)
{
    if(strings.empty()) {
        throw __FILE__ " Found no commands in the plugin";
    }

    for(int i = 0; i < strings.size(); i++) {
        pp->addCommand(trimString(strings[i]));
    }
}

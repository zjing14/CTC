#include "ctc_engine.h"
#include "ctc_workflow.h"
#include "ctc_util.h"
#include "ctc_plugin.h"
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
using namespace std;


int id_index = 0;
int input_index = 1;
int output_index = 1;

void add_input(Workflow &w)
{
    string name, type, format;
    Parameter p;
    string cmd;
    while(1) {
        cout << "1) list workflow inputs" << endl;
        cout << "2) add workflow inputs" << endl;
        cout << "3) del workflow inputs" << endl;
        cout << "0) return back" << endl;
        cout << ">>";
        getline(cin, cmd);
        char cmd_id = cmd[0];
        switch(cmd_id) {
        case '0':
            return;
        case '1':
            cout << "----------------Workflow Inputs----------------\n";
            w.printInput();
            cout << "-----------------------------------------------\n\n";
            break;
        case '2':
            while(1) {
                cout << "Input_" << input_index << endl;
                cout << "  name: ";
                getline(cin, name);
                if(name == "") {
                    break;
                }

                cout << "  type(file, bool, float, int, text): ";
                getline(cin, type);
                if(type == "file") {
                    p.para_type = DATA_FILE;
                } else if(type == "bool") {
                    p.para_type = BOOL;
                } else if(type == "float") {
                    p.para_type = FLOAT;
                } else if(type == "int") {
                    p.para_type = INT;
                } else if(type == "text") {
                    p.para_type = TEXT;
                } else {
                    p.para_type = UNKNOWN;
                    cout << "  Warning: Unkown type" << endl;
                }

                if(p.para_type == DATA_FILE) {
                    cout << "  format: ";
                    getline(cin, format);
                    p.format = format;
                }

                if(w.findInput(name)) {
                    cout << "  the input name already exist!" << endl;
                } else {
                    w.addInput(name, p);
                    input_index++;
                }
            }
            break;
        case '3':
            cout << "input name: ";
            getline(cin, name);
            if(w.delInput(name)) {
                cout << "Successfully deleted input: " << name << endl;
                input_index--;
            } else {
                cout << "Not found input: " << name << endl;
            }
            break;
        }
    }
}

void add_output(Workflow &w)
{
    string name, type, format, value;
    Parameter p;
    string cmd;
    while(1) {
        cout << "1) list workflow outputs" << endl;
        cout << "2) print workflow" << endl;
        cout << "3) add workflow outputs" << endl;
        cout << "4) del workflow outputs" << endl;
        cout << "0) return back" << endl;
        cout << ">>";
        getline(cin, cmd);
        char cmd_id = cmd[0];
        switch(cmd_id) {
        case '0':
            return;
        case '1':
            cout << "----------------Workflow Outputs----------------\n";
            w.printOutput();
            cout << "- ----------------------------------------------\n\n";
            break;
        case '2':
            cout << "----------------Workflow----------------\n";
            w.print();
            cout << "----------------------------------------\n\n";
            break;
        case '3':
            while(1) {
                cout << "Output_" << output_index << endl;
                cout << "  name: ";
                getline(cin, name);
                if(name == "") {
                    break;
                }
                cout << "  type(file, bool, float, int, text): ";
                getline(cin, type);
                if(type == "file") {
                    p.para_type = DATA_FILE;
                } else if(type == "bool") {
                    p.para_type = BOOL;
                } else if(type == "float") {
                    p.para_type = FLOAT;
                } else if(type == "int") {
                    p.para_type = INT;
                } else if(type == "text") {
                    p.para_type = TEXT;
                } else {
                    p.para_type = UNKNOWN;
                    cout << "  Warning: Unkown type" << endl;
                }

                if(p.para_type == DATA_FILE) {
                    cout << "  format: ";
                    getline(cin, format);
                    p.format = format;
                }
                while(1) {
                    cout << "  value (? for options): ";
                    getline(cin, value);
                    if(value == "?") {
                        cout << "----------------Step Outputs----------------\n";
                        w.printStepOutput();
                        cout << "--------------------------------------------\n";
                    } else {
                        break;
                    }
                }
                bool value_exist = false;
                size_t pos = value.find(".");
                if(pos != string::npos) {
                    string sid = value.substr(0, pos);
                    string vname = value.substr(pos + 1, value.size());
                    value_exist = w.findStepOutput(sid, vname);
                }

                if(value_exist) {
                    if(w.findOutput(name)) {
                        cout << "  the output name already exist!" << endl;
                    } else {
                        p.val = "$" + value;
                        w.addOutput(name, p);
                        output_index++;
                    }
                } else {
                    cout << "  the value is invalid!" << endl;
                }

            }
            break;
        case '4':
            cout << "output name: ";
            getline(cin, name);
            if(w.delOutput(name)) {
                cout << "Successfully deleted output: " << name << endl;
                output_index--;
            } else {
                cout << "Not found output: " << name << endl;
            }
            break;
        }
    }
}

void add_step(Workflow &w, Engine &e)
{
    string cmd;


    while(1) {
        string id, plugin_id;
        string name, value;
        cout << "1) list workflow inputs" << endl;
        cout << "2) list workflow outputs" << endl;
        cout << "3) print workflow" << endl;
        cout << "4) list plugins" << endl;
        cout << "5) add a step" << endl;
        cout << "6) del a step" << endl;
        cout << "0) return back" << endl;
        cout << ">>";
        getline(cin, cmd);
        char cmd_id = cmd[0];

        switch(cmd_id) {
        case '0':
            return;
        case '1':
            cout << "----------------Workflow Inputs----------------\n";
            w.printInput();
            cout << "-----------------------------------------------\n\n";
            break;
        case '2':
            cout << "----------------Workflow Outputs----------------\n";
            w.printOutput();
            cout << "------------------------------------------------\n\n";
            break;
        case '3':
            cout << "----------------Workflow----------------\n";
            w.print();
            cout << "----------------------------------------\n\n";
            break;
        case '4':
            cout << "----------------Plugins----------------\n";
            e.listPlugins();
            cout << "---------------------------------------\n\n";
            break;
        case '6':
            cout << "step id: ";
            getline(cin, id);
            if(w.delStep(id)) {
                cout << "Successfully deleted step: " << id << endl;
            } else {
                cout << "Not found step: " << id << endl;
            }
            break;
        case '5':
            map< string, string > inputs;
            map< string, string > outputs;
            id_index++;
            char id_chr[10];
            sprintf(id_chr, "%d", id_index);
            id = id_chr;
            while(1) {
                cout << "id: " << id_index <<  " plugin_id (? for options): ";
                getline(cin, plugin_id);
                if(plugin_id == "?") {
                    cout << "----------------Plugins----------------\n";
                    e.listPlugins();
                    cout << "---------------------------------------\n\n";
                } else {
                    break;
                }
            }
            if(!e.findPlugin(plugin_id)) {
                cout << " Not found the plugin" << endl;
                break;
            }

            cout << "Inputs: " << endl;
            int l_output_index = 1;
            int l_input_index = 1;
            while(1) {
                cout << "input_" << l_input_index << endl;
                while(1) {
                    cout << "  name (? for options): ";
                    getline(cin, name);
                    if(name == "?") {
                        cout << "----------------Plugin Inputs----------------\n";
                        e.listPluginInput(plugin_id);
                        cout << "----------------------------------------------\n";
                    } else {
                        break;
                    }
                }
                if(name == "") {
                    break;
                }
                if(!e.findPluginInput(plugin_id, name)) {
                    cout << "  Invalid name!" << endl;
                    continue;
                }
                while(1) {
                    cout << "  value (? for options): ";
                    getline(cin, value);
                    if(value == "?") {
                        cout << "----------------Workflow Inputs----------------\n";
                        w.printInput();
                        cout << "------------------Step Outputs-----------------\n";
                        w.printStepOutput();
                        cout << "-----------------------------------------------\n";
                    } else {
                        break;
                    }
                }
                bool value_exist = false;
                size_t pos = value.find(".");
                if(pos != string::npos) {
                    string sid = value.substr(0, pos);
                    string vname = value.substr(pos + 1, value.size());
                    value_exist = w.findStepOutput(sid, vname);
                } else {
                    // Consider this is a valid value for now
                    value_exist = true;
                    // value_exist = w.findInput(value);
                }

                if(value_exist) {
                    if(inputs.find(name) != inputs.end()) {
                        cout << "  the input name already exist!" << endl;
                    } else {
                        inputs[name] = "$" + value;
                        l_input_index++;
                    }
                } else {
                    cout << "  value is invalid!" << endl;
                }
            }
            cout << "Output: " << endl;
            while(1) {
                cout << "output_" << l_output_index << endl;
                while(1) {
                    cout << "  name (? for options): ";
                    getline(cin, name);
                    if(name == "?") {
                        cout << "----------------Plugin Outputs----------------" << endl;;
                        e.listPluginOutput(plugin_id);
                        cout << "----------------------------------------------\n";
                    } else {
                        break;
                    }
                }
                if(name == "") {
                    break;
                }
                if(!e.findPluginOutput(plugin_id, name)) {
                    cout << "  Invalid name!" << endl;
                    continue;
                }
                //                    while(1){
                //                        cout << "  value: ";
                //                        getline(cin, value);
                //                        if(value == "")
                //                            cout << "  the value cannot be empty!" << endl;
                //                        else
                //                            break;
                //                    }
                value = "";
                if(outputs.find(name) != outputs.end()) {
                    cout << "  the output name already exist!" << endl;
                } else {
                    outputs[name] = value;
                    l_output_index++;
                }

            }
            w.addStep(INT, id, plugin_id, inputs, outputs);
            break;
        }
    }
}

int main(int argc, char **argv)
{
    if(argc != 3) {
        cerr << "Usage: " << argv[0] << " <engine_root> <workflow_id>" << endl;
        return -1;
    }

    Engine engine(argv[1]);
    Catalog *catalog = Catalog::instance();

    string id = argv[2];
    string name, version, description;
    string cmd;

    if(catalog->hasWorkflow(id)) {
        cerr << "Workflow with the same id exists" << endl;
        return -1;
    }

    cout << "Name: ";
    getline(cin, name);
    cout << "Version: ";
    getline(cin, version);
    cout << "Description: ";
    getline(cin, description);
    Workflow w(id, name, version, description);

    while(1) {
        cout << endl;
        cout << "1) EDIT INPUTS" << endl;
        cout << "2) EDIT STEPS" << endl;
        cout << "3) EDIT OUTPUTS" << endl;
        cout << "4) PRINT WORKFLOW" << endl;
        cout << "5) SAVE WORKFLOW" << endl;
        cout << "0) EXIT" << endl;
        cout << ">>";
        getline(cin, cmd);
        cout << "\n";

        bool exit = false;
        string suite_id;
        string w_conf;
        switch(cmd[0]) {
        case '1':
            cout << "Edit Inputs: " << endl;
            add_input(w);
            break;
        case '2':
            cout << "Edit Step: " << endl;
            add_step(w, engine);
            break;
        case '3':
            cout << "Edit Outputs: " << endl;
            add_output(w);
            break;
        case '4':
            cout << "--------------Workflows--------------" << endl;
            w.print();
            cout << "-------------------------------------" << endl;
            break;
        case '5':
            while(1) {
                cout << "Suite ID (? for options): ";
                getline(cin, suite_id);
                if(suite_id == "?") {
                    cout << "----------------Listing Suites----------------\n";
                    engine.listSuites();
                    cout << "--------------------------------------------\n";
                } else {
                    break;
                }
            }
            w_conf = SysConfig::instance()->getWorkflowDir() + "/" + w.getID() + ".xml";
            if(catalog->addWorkflow(suite_id, w.getID(), w_conf)) {
                cerr << "Error adding workflow to the catalog" << endl;
            } else {
                cout << "Writing to: " << w_conf << endl;
                w.buildXml(w_conf);
                catalog->updateConfigFile();
            }

            break;
        case '0':
            exit = true;
            break;
        default:
            break;
        }
        if(exit) {
            break;
        }
    }

}

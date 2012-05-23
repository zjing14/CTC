#include "ctc_engine.h"
#include "ctc_plugin.h"
#include "ctc_workflow.h"
#include "ctc_logger.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

Engine::Engine(const string &path, SysConfig *sys_conf)
{
    if(path.c_str()[0] != '/') {
        _root_dir = getCurrentDir() + '/' + path;
    } else {
        _root_dir = path;
    }

    if(path.c_str()[path.length() - 1] != '/') {
        _root_dir += '/';
    }

    if(sys_conf == NULL) {
        // Init from ctc.ini
        string conf_file = _root_dir + "ctc.ini";
        sys_conf = SysConfig::instance();
        sys_conf->openConfigFile(conf_file);
    }

    // Create working directory
    string work_dir = sys_conf->getWorkDir();
    if(work_dir.c_str()[0] != '/') {
        work_dir = _root_dir + work_dir;
        sys_conf->setWorkDir(work_dir);
    }
    createDir(work_dir);

    // Create plugin directory
    string plugin_dir = sys_conf->getPluginDir();
    if(plugin_dir.c_str()[0] != '/') {
        plugin_dir = _root_dir + plugin_dir;
        sys_conf->setPluginDir(plugin_dir);
    }
    createDir(plugin_dir);

    // Create workflow directory
    string workflow_dir = sys_conf->getWorkflowDir();
    if(workflow_dir.c_str()[0] != '/') {
        workflow_dir = _root_dir + workflow_dir;
        sys_conf->setWorkflowDir(workflow_dir);
    }
    createDir(workflow_dir);

    // Create log file
    string log_file = sys_conf->getLogFileName();
    Logger *logger = Logger::instance();
    if(!log_file.empty()) {
        logger->openLogFile(log_file);
    }

    // Open catalog, create an empty one if it does not exist
    string catalog_file = _root_dir + "catalog.xml";
    _catalog = Catalog::instance();
    _catalog->openConfigFile(_root_dir, catalog_file);

    _sys_paras["ENGINE_DIR"] = _root_dir;
}

Engine::~Engine()
{
    _sys_paras.clear();
}

void Engine::listPlugins()
{
    vector <string> ids;
    _catalog->getPluginIDs(ids);
    for(int i = 0; i < ids.size(); i++) {
        string conf_file = _catalog->getPluginFile(ids[i]);
        Plugin plugin(conf_file);
        cout << plugin.getInfo() << endl;
    }
}

void Engine::listPluginOutput(const string &id)
{
    string conf_file = _catalog->getPluginFile(id);
    Plugin plugin(conf_file);
    plugin.printOutputs();
}

void Engine::listPluginInput(const string &id)
{
    string conf_file = _catalog->getPluginFile(id);
    Plugin plugin(conf_file);
    plugin.printInputs();
}
bool Engine::findPluginOutput(const string &id, const string &name)
{
    string conf_file = _catalog->getPluginFile(id);
    Plugin plugin(conf_file);
    return plugin.findOutput(name);
}
bool Engine::findPluginInput(const string &id, const string &name)
{
    string conf_file = _catalog->getPluginFile(id);
    Plugin plugin(conf_file);
    return plugin.findInput(name);
}
void Engine::listWorkflows()
{
    vector <string> ids;
    _catalog->getWorkflowIDs(ids);

    for(int i = 0; i < ids.size(); i++) {
        string conf_file = _catalog->getWorkflowFile(ids[i]);
        Workflow workflow(conf_file);
        cout << workflow.getInfo() << endl;
    }
}

void Engine::listSuites()
{
    _catalog->listSuites(_root_dir);
}

void Engine::queryPlugin(const string &id)
{
    Plugin plugin(_catalog->getPluginFile(id));
    plugin.print();
}

void Engine::queryWorkflow(const string &id)
{
    Workflow workflow(_catalog->getWorkflowFile(id));
    workflow.print();
}

int Engine::executePlugin(const string &id, map < string, string >& paras, bool dry_run)
{
    Plugin plugin(_catalog->getPluginFile(id));
    vector <string> commands;

    string workdir = getTmpDir(SysConfig::instance()->getWorkDir());
    string org_dir = getCurrentDir();
    // Change current working directory
    DBG_LOG << "Changing working directory to " << workdir << endl;
    if(changeCurrentDir(workdir)) {
        throw __FILE__ " Error changing working directory";
    }

    try {
        vector <string> cmds;
        plugin.getExecCommands(cmds, paras, _sys_paras);

        for(int i = 0; i < cmds.size(); i++) {
            DBG_LOG << "Executing: \"" << cmds[i] << "\"" << endl;
            if(!dry_run) {
                system(cmds[i].c_str());
            }
        }
    } catch(const char *err) {
        cerr << "Error executing plugin " << id << " " << err << endl;
    }
    // Recover current dir
    DBG_LOG << "Changing working directory to " << workdir << endl;
    if(changeCurrentDir(org_dir)) {
        throw __FILE__ " Error changing directory";
    }
    // Cleanup temporary directory
    removeDir(workdir);

    return 0;
}

int Engine::executeWorkflow(const string &id, map < string, string >& paras, bool dry_run)
{
    string conf_file = _catalog->getWorkflowFile(id);
    Workflow workflow(conf_file);
    executeWorkflow(workflow, paras, dry_run);
    return 0;
}

int Engine::executeWorkflow(Workflow &workflow, map < string, string >& paras, bool dry_run)
{
    std::map <std::string, std::string> exec_paras;

    map <string, map <string, string> > step_paras;
    // Initialize step configurations
    for(list< WorkflowStep >::iterator it = workflow._steps.begin(); it != workflow._steps.end(); it++) {
        step_paras[(*it).id] = map<string, string>();
    }

    // Add workflow-level variables.
    // TODO: Reset workflow-level variables after executing a sub-workflow to avoid conflict
    for(map<string, string>::iterator it = workflow._defines.begin(); it != workflow._defines.end(); it++) {
        _sys_paras[it->first] = it->second;
    }

    map < string, string >::iterator mit;
    for(mit = paras.begin(); mit != paras.end(); mit++) {
        size_t pos = mit->first.find(".");
        if(pos != string::npos) {
            // This is a parameter for a step
            string sid = mit->first.substr(0, pos);
            if(step_paras.find(sid) == step_paras.end()) {
                DBG_LOG << "Cannot find matched step for parameter " << mit->first << endl;
                throw __FILE__ " Error parsing input parameters";
            }
            // Strip out the step id from the parameter
            step_paras[sid][mit->first.substr(pos + 1)] = mit->second;
        } else {
            exec_paras[mit->first] = mit->second;
        }
    }

    string workdir = getTmpDir(SysConfig::instance()->getWorkDir());
    string org_dir = getCurrentDir();
    // Change current working directory
    DBG_LOG << "Changing working directory to " << workdir << endl;
    if(changeCurrentDir(workdir)) {
        throw __FILE__ " Error changing working directory";
    }

    bool has_error = false;
    try {
        for(list< WorkflowStep >::iterator it = workflow._steps.begin(); it != workflow._steps.end(); it++) {
            WorkflowStep &step = *it;
            map <string, string>::iterator it;
            map <string, string>& s_paras = step_paras[step.id];
            for(it = step.inputs.begin(); it != step.inputs.end(); it++) {
                string var_name = it->first;
                string value = it->second;

                // Allow overriding of default values
                if(s_paras.find(var_name) == s_paras.end()) {
                    s_paras[var_name] = resolveWord(value, exec_paras, _sys_paras);
                }
            }

            Plugin *p;
            Workflow *w;

            if(step.type == PLUGIN) {
                string conf_file = _catalog->getPluginFile(step.plugin_id);
                p = new Plugin(conf_file);
            } else if(step.type == WORKFLOW) {
                string conf_file = _catalog->getWorkflowFile(step.plugin_id);
                w = new Workflow(conf_file);
            } else {
                throw __FILE__ " Invalid step type";
            }

            // Generate temporary output file names for each step output
            for(it = step.outputs.begin(); it != step.outputs.end(); it++) {
                string var_name = it->first;
                ostringstream oss_name; // runtime name
                oss_name << step.id << "." << var_name;
                ostringstream oss_val;
                oss_val << step.id << "-" << step.plugin_id << "-" << var_name << ".tmp";
                string format;
                if(step.type == PLUGIN) {
                    format = p->getParameter(var_name).format;
                } else if(step.type == WORKFLOW) {
                    format = w->getParameter(var_name).format;
                } else {
                    throw __FILE__ " Invalid step type";
                }
                if(format != "") {
                    oss_val << "." << format;
                }
                step_paras[step.id][var_name] = workdir + "/" + oss_val.str();
                // Register output name for use in following steps
                exec_paras[oss_name.str()] = oss_val.str();
            }

            // Execute a step
            if(step.type == PLUGIN) {
                vector <string> step_cmds;
                p->getExecCommands(step_cmds, step_paras[step.id], _sys_paras);
                for(int i = 0; i < step_cmds.size(); i++) {
                    DBG_LOG << "Executing: " << step_cmds[i] << endl;
                    if(!dry_run) {
                        system(step_cmds[i].c_str());
                    }
                }
                delete p;
            } else if(step.type == WORKFLOW) {
                executeWorkflow(step.plugin_id, step_paras[step.id], dry_run);
                delete w;
            }
        }

        // Move temporary files to final outputs
        map < string, Parameter >::iterator mit1;
        for(mit1 = workflow._outputs.begin(); mit1 != workflow._outputs.end(); mit1++) {
            string value = (mit1->second).val;
            string source = resolveWord(value, exec_paras, _sys_paras);
            string dest = exec_paras[mit1->first];
            if(dest.c_str()[0] != '/') {
                dest = org_dir + '/' + dest;
            }
            DBG_LOG << "Moving file from " << source << " to " << dest << endl;
            if(!dry_run) {
                if(moveFile(source, dest)) {
                    throw __FILE__ " Error generating final output";
                }
            }
        }
    } catch(const char *err) {
        cerr << err << endl;
        cerr << "Error executing workflow " << workflow._id << endl;
    }

    // Recover current dir
    DBG_LOG << "Changing working directory to " << org_dir << endl;
    if(changeCurrentDir(org_dir)) {
        throw __FILE__ " Error changing working directory";
    }

    if(!has_error) {
        // Cleanup temporary directory
        // DBG_LOG << "Removing " << workdir << endl;
        // system(("rm -rf " + workdir).c_str());
    }

    return 0;
}

bool Engine::findPlugin(const std::string &id)
{
    vector <string> ids;
    _catalog->getPluginIDs(ids);
    for(std::vector<string>::iterator it = ids.begin(); it != ids.end(); it++)
        if(id == *it) {
            return true;
        }
    return false;
}


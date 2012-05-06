#ifndef __ctc_engine_h__
#define __ctc_engine_h__

#include <string>
#include <map>
#include "ctc_workflow.h"
#include "ctc_catalog.h"
#include "ctc_sysconfig.h"

class Engine
{
public:
    Engine(const std::string &path, SysConfig *sys_config = NULL);
    ~Engine();

    void listPlugins();
    void listPluginInput(const std::string &id);
    void listPluginOutput(const std::string &id);
    void listWorkflows();
    void listSuites();
    bool findPluginOutput(const std::string &id, const std::string &name);
    bool findPluginInput(const std::string &id, const std::string &name);

    void queryPlugin(const std::string &id);
    void queryWorkflow(const std::string &id);

    int executePlugin(const std::string &id, std::map < std::string, std::string >& paras, bool dry_run = false);
    int executeWorkflow(const std::string &id, std::map < std::string, std::string >& paras, bool dry_run = false);
    int executeWorkflow(Workflow &workflow, std::map < std::string, std::string >& paras, bool dry_run = false);
    bool findPlugin(const std::string &id);

protected:
    Catalog *_catalog;
    std::string _root_dir;
    std::map <std::string, std::string> _sys_paras; // System-wide configuration
};

#endif

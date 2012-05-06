#ifndef __ctc_workflow_h__
#define __ctc_workflow_h__

#include <string>
#include <vector>
#include <list>
#include <map>
#include <ctc_util.h>

struct WorkflowStep {
    int type;
    std::string id;
    std::string plugin_id;
    std::map < std::string, std::string > inputs;
    std::map < std::string, std::string > outputs;

    WorkflowStep() {}
    WorkflowStep(int type, std::string id, std::string plugin_id);
    void addInput(const std::string &var_name, const std::string &value);
    void addOutput(const std::string &var_name);
};

class Workflow
{
public:
    Workflow() {}
    Workflow(const std::string &conf_file);
    Workflow(const std::string &id, const std::string &name, const std::string &version, const std::string &description);
    Parameter getParameter(const std::string &var);
    void print();
    std::string getInfo();
    void buildXml(const std::string &conf_file);
    void addStep(WorkflowStep &ws);
    void addStep(int type, const std::string &id, const std::string &plugin_id, std::map< std::string, std::string > &inputs, std::map< std::string, std::string > &outputs);
    bool delStep(const std::string &id);
    bool findStepOutput(const std::string &id, const std::string &name);
    void addInput(const std::string &name, Parameter &p);
    bool delInput(const std::string &name);
    bool findInput(const std::string &name);
    void addOutput(const std::string &name, Parameter &p);
    bool delOutput(const std::string &name);
    bool findOutput(const std::string &name);
    void addVar(const std::string &name, const std::string &value);
    void printInput();
    void printOutput();
    void printStepOutput();

    std::string getID() {
        return _id;
    }
    std::string getName() {
        return _name;
    }
    std::string getVersion() {
        return _version;
    }
    std::string getDesc() {
        return _desc;
    }

    void setID(const std::string &id) {
        _id = id;
    }
    void setName(const std::string &name) {
        _name = name;
    }
    void setVersion(const std::string &version) {
        _version = version;
    }
    void setDesc(const std::string &desc) {
        _desc = desc;
    }

    friend class Engine;

protected:
    std::string _id;
    std::string _name;
    std::string _version;
    std::string _desc;
    std::list < WorkflowStep > _steps;

    std::map < std::string, std::string > _defines;
    std::map < std::string, Parameter > _inputs;
    std::map < std::string, Parameter > _outputs;
};

#endif

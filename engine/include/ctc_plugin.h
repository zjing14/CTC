#ifndef __ctc_plugin_h__
#define __ctc_plugin_h__

#include <string>
#include <map>
#include <vector>
#include <ctc_util.h>

class Plugin
{
public:
    Plugin() {}
    Plugin(const std::string &conf_file);
    ~Plugin();

    void getExecCommands(std::vector <std::string>& exec_cmds, std::map < std::string, std::string >& exec_paras, std::map < std::string, std::string >& sys_paras);
    std::string getExecCommand(const std::string &interp, const std::string &command, std::map < std::string, std::string >& exec_paras, std::map < std::string, std::string >& sys_paras);
    std::string getInfo();
    Parameter getParameter(const std::string &var);
    void printInputs();
    void printOutputs();
    void print();
    bool findInput(const std::string &name);
    bool findOutput(const std::string &name);
    void buildXml(const std::string &conf_file);

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
        return _description;
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
        _description = desc;
    }

    void addInput(const std::string &name, Parameter &p);
    void addOutput(const std::string &name, Parameter &p);
    void addCommand(const std::string &cmd);

protected:
    std::string _conf_dir;
    std::string _name;
    std::string _id;
    std::string _version;
    std::string _description;
    std::vector < std::string > _commands;
    std::vector < std::string > _command_interps;
    std::map < std::string, Parameter > _inputs;
    std::map < std::string, Parameter > _outputs;
};

#endif

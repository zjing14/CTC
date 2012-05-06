#include <iostream>
#include <fstream>
#include "ctc_plugin.h"
#include "ctc_util.h"
#include "ctc_logger.h"
#include "ctc_sysconfig.h"
#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"
#include <cstring>
#include <string>
#include <sstream>

using namespace std;
using namespace rapidxml;

Plugin::Plugin(const string &conf_file)
{
    size_t pos = conf_file.rfind('/');
    if(pos != string::npos) {
        _conf_dir = conf_file.substr(0, pos + 1);
    }

    xml_document<> doc;    // character type defaults to char
    char *cstr = getFileContent(conf_file);
    doc.parse<0>(cstr);

    xml_node<> *root = doc.first_node();
    xml_node<> *child;
    _name = root->first_attribute("name")->value();
    _id = root->first_attribute("id")->value();
    _version = root->first_attribute("version")->value();
    _description = root->first_node("description")->value();
    child = root->first_node("commands");

    for(xml_node<> *comm = child->first_node(); comm; comm = comm->next_sibling()) {
        xml_attribute<> *attr = child->first_attribute("interpreter");
        if(attr) {
            _command_interps.push_back(attr->value());
        } else {
            _command_interps.push_back("");
        }

        _commands.push_back(comm->value());
    }
    child = root->first_node("inputs");
    for(xml_node<> *param = child->first_node(); param; param = param->next_sibling()) {
        Parameter p;
        xml_attribute<> *attr = param->first_attribute("type");
        string type = attr->value();
        if(type == string("file")) {
            p.para_type = DATA_FILE;
        } else if(type == string("bool")) {
            p.para_type = BOOL;
        } else if(type == string("float")) {
            p.para_type = FLOAT;
        } else if(type == string("int")) {
            p.para_type = INT;
        } else if(type == string("text")) {
            p.para_type = TEXT;
        } else {
            p.para_type = UNKNOWN;
        }

        if(p.para_type == DATA_FILE) {
            p.format = param->first_attribute("format")->value();
        }

        if(param->first_attribute("value")) {
            p.val = param->first_attribute("value")->value();
        }

        p.label = param->first_attribute("label")->value();
        _inputs[param->first_attribute("name")->value()] = p;
    }

    child = root->first_node("outputs");
    for(xml_node<> *param = child->first_node(); param; param = param->next_sibling()) {
        Parameter p;

        xml_attribute<> *attr = param->first_attribute("type");

        if(!attr) {
            p.para_type == DATA_FILE;
        } else {
            string type = attr->value();
            if(type == string("file")) {
                p.para_type = DATA_FILE;
            } else if(type == string("bool")) {
                p.para_type = BOOL;
            } else if(type == string("float")) {
                p.para_type = FLOAT;
            } else if(type == string("int")) {
                p.para_type = INT;
            } else if(type == string("text")) {
                p.para_type = TEXT;
            } else {
                p.para_type = UNKNOWN;
            }
        }
        if(p.para_type == DATA_FILE) {
            attr = param->first_attribute("format");
            p.format = attr->value();
        }
        p.label = param->first_attribute("label")->value();
        _outputs[param->first_attribute("name")->value()] = p;
    }
    delete cstr;
    doc.clear();
}

void Plugin::buildXml(const string &conf_file)
{
    xml_document<> doc;
    xml_node<>* root = doc.allocate_node(node_element, "plugin");
    doc.append_node(root);
    root->append_attribute(doc.allocate_attribute("id", _id.c_str()));
    root->append_attribute(doc.allocate_attribute("name", _name.c_str()));
    root->append_attribute(doc.allocate_attribute("version", _version.c_str()));
    xml_node<>* child = doc.allocate_node(node_element, "description");
    child->value(_description.c_str());
    root->append_node(child);
    child = doc.allocate_node(node_element, "commands");
    vector < string >::const_iterator itt = _command_interps.begin();
    for(vector < string >::const_iterator it = _commands.begin(); it != _commands.end(); it++, itt++) {
        xml_node<>* cc = doc.allocate_node(node_element, "command");
        cc->value((*it).c_str());
        if(!(*itt).empty()) {
            cc->append_attribute(doc.allocate_attribute("interpreter", (*itt).c_str()));
        }
        child->append_node(cc);
    }
    root->append_node(child);
    child = doc.allocate_node(node_element, "inputs");
    for(map < string, Parameter >::const_iterator it = _inputs.begin(); it != _inputs.end(); it++) {
        xml_node<>* cc = doc.allocate_node(node_element, "param");
        cc->append_attribute(doc.allocate_attribute("name", (*it).first.c_str()));
        cc->append_attribute(doc.allocate_attribute("type", para_types[(*it).second.para_type].c_str()));
        if((*it).second.para_type == DATA_FILE) {
            cc->append_attribute(doc.allocate_attribute("format", (*it).second.format.c_str()));
        }
        cc->append_attribute(doc.allocate_attribute("label", (*it).second.label.c_str()));
        child->append_node(cc);
    }
    root->append_node(child);
    child = doc.allocate_node(node_element, "outputs");
    for(map < string, Parameter >::const_iterator it = _outputs.begin(); it != _outputs.end(); it++) {
        xml_node<>* cc = doc.allocate_node(node_element, "data");
        cc->append_attribute(doc.allocate_attribute("name", (*it).first.c_str()));
        cc->append_attribute(doc.allocate_attribute("type", para_types[(*it).second.para_type].c_str()));
        if((*it).second.para_type == DATA_FILE) {
            cc->append_attribute(doc.allocate_attribute("format", (*it).second.format.c_str()));
        }
        cc->append_attribute(doc.allocate_attribute("label", (*it).second.label.c_str()));
        child->append_node(cc);
    }
    root->append_node(child);
    ofstream ifs(conf_file.c_str());
    if(!ifs.is_open()) {
        throw __FILE__ " getFileContent(): Cannot open file";
    }
    ifs << doc;
    ifs.close();
    doc.clear();
}

void Plugin::getExecCommands(vector < string >& exec_cmds, map < string, string >& exec_paras, map <string, string>& sys_paras)
{
    sys_paras["PLUGIN_DIR"] = _conf_dir;

    for(int i = 0; i < _commands.size(); i++) {
        exec_cmds.push_back(getExecCommand(_command_interps[i], _commands[i], exec_paras, sys_paras));
    }
}

string Plugin::getExecCommand(const string &interp, const string &command, map < string, string >& exec_paras, map <string, string>& sys_paras)
{
    string exec_cmd;

    // Validity check of parameters. Currently only check if the parameter has been defined.
    map < string, string >::iterator mit;
    for(mit = exec_paras.begin(); mit != exec_paras.end(); mit++) {
        if(_inputs.find(mit->first) == _inputs.end() && _outputs.find(mit->first) == _outputs.end()) {
            cout << "Cannot find parameter " << mit->first << endl;
            throw __FILE__ " Plugin::getExecCommand(): Invalid parameters!";
        }
    }

    // Add default values of parameters if they are not defined in the incoming configuration
    map < string, Parameter >::iterator mit1;
    for(mit1 = _inputs.begin(); mit1 != _inputs.end(); mit1++) {
        if(exec_paras.find(mit1->first) == exec_paras.end()) {
            exec_paras[mit1->first] = (mit1->second).val;
        }
    }
    for(mit1 = _outputs.begin(); mit1 != _outputs.end(); mit1++)
        if(exec_paras.find(mit1->first) == exec_paras.end()) {
            exec_paras[mit1->first] = (mit1->second).val;
        }

    // Generate command
    if(!interp.empty()) {
        exec_cmd += interp + " ";
    }

    return exec_cmd + resolveStream(command, exec_paras, sys_paras);
}
bool Plugin::findInput(const string &name)
{
    if(_inputs.find(name) != _inputs.end()) {
        return true;
    }
    return false;
}

bool Plugin::findOutput(const string &name)
{
    if(_outputs.find(name) != _outputs.end()) {
        return true;
    }
    return false;
}
string Plugin::getInfo()
{
    ostringstream oss;
    oss << "ID: \"" << _id << "\", Name: \"" << _name << "\", Version: \"" << _version << "\"";
    return oss.str();
}

Parameter Plugin::getParameter(const std::string &var)
{
    if(_inputs.find(var) != _inputs.end()) {
        return _inputs[var];
    }
    if(_outputs.find(var) != _outputs.end()) {
        return _outputs[var];
    }
    DBG_LOG << "Cannot find variable " << var << endl;
    throw __FILE__ " Variable not found";
}

void Plugin::printInputs()
{
    ostringstream oss;
    oss << "Inputs:" << endl;
    for(map < string, Parameter >::const_iterator it = _inputs.begin(); it != _inputs.end(); it++)
        oss << "\tname=\"" << (*it).first << "\" type=\"" << para_types[(*it).second.para_type] << "\""
            << " format=\"" << (*it).second.format  << "\" label=\"" << (*it).second.label << "\"" << endl;
    cout << oss.str();
}

void Plugin::printOutputs()
{
    ostringstream oss;
    oss << "Outputs:" << endl;
    for(map < string, Parameter >::const_iterator it = _outputs.begin(); it != _outputs.end(); it++)
        oss << "\tname=\"" << (*it).first << "\" type=\"" << para_types[(*it).second.para_type] << "\""
            << " format=\"" << (*it).second.format  << "\" label=\"" << (*it).second.label << "\"" << endl;
    cout << oss.str();
}

void Plugin::print()
{
    ostringstream oss;

    oss << "ID:" << _id << endl;
    oss << "Name:" << _name << endl;
    oss << "Version:" << _version << endl;
    oss << "Description:" << _description << endl;
    oss << "Commands:" << endl;
    vector < string >::const_iterator itt = _command_interps.begin();
    for(vector < string >::const_iterator it = _commands.begin(); it != _commands.end(); it++, itt++) {
        oss << (*itt) << (*it) << endl;
    }
    cout << oss.str() << endl;

    printInputs();
    printOutputs();
}

void
Plugin::addInput(const string &name, Parameter &p)
{
    _inputs[name] = p;
}

void
Plugin::addOutput(const string &name, Parameter &p)
{
    _outputs[name] = p;
}

Plugin::~Plugin()
{
}

void Plugin::addCommand(const string &cmd)
{
    _command_interps.push_back("");
    _commands.push_back(cmd);
}

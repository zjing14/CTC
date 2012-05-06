#include <iostream>
#include <fstream>
#include <sstream>
#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"
#include "ctc_workflow.h"
#include "ctc_util.h"
#include "ctc_sysconfig.h"
#include "ctc_logger.h"
#include <cstring>
#include <string>

using namespace std;
using namespace rapidxml;

WorkflowStep::WorkflowStep(int type, string id, string plugin_id)
{
    this->type = type;
    this->id = id;
    this->plugin_id = plugin_id;
}

void Workflow::addVar(const std::string &name, const std::string &value)
{
    _defines[name] = value;
}

void WorkflowStep::addInput(const string &var_name, const string &value)
{
    inputs[var_name] = value;
}


void WorkflowStep::addOutput(const string &var_name)
{
    outputs[var_name] = "";
}

Workflow::Workflow(const string &conf_file)
{
    xml_document <> doc;        // character type defaults to char
    char *cstr = getFileContent(conf_file);
    doc.parse < 0 > (cstr);

    xml_node <> *root = doc.first_node();
    _id = root->first_attribute("id")->value();
    _name = root->first_attribute("name")->value();
    _version = root->first_attribute("version")->value();
    _desc = root->first_node("description")->value();

    for(xml_node <> *input = root->first_node("inputs")->first_node(); input;
            input = input->next_sibling()) {
        Parameter p;
        xml_attribute <> *attr;
        attr = input->first_attribute("type");
        string type = attr->value();
        if(type == string("file")) {
            p.para_type = DATA_FILE;
        } else if(type == string("boolen")) {
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
            p.format = input->first_attribute("format")->value();
        }

        if(input->first_attribute("value")) {
            p.val = input->first_attribute("value")->value();
        }

        _inputs[input->first_attribute("name")->value()] = p;
    }

    for(xml_node <> *step = root->first_node("steps")->first_node(); step;
            step = step->next_sibling()) {
        WorkflowStep s;
        s.id = step->first_attribute("id")->value();
        string t = step->first_attribute("type")->value();
        if(t == string("plugin")) {
            s.type = PLUGIN;
        } else if(t == string("workflow")) {
            s.type = WORKFLOW;
        } else {
            s.type = WS_UNKNOWN;
        }
        s.plugin_id = step->first_attribute("plugin_id")->value();

        for(xml_node <> *input = step->first_node("inputs")->first_node();
                input; input = input->next_sibling()) {
            s.inputs[input->first_attribute("name")->value()] =
                input->first_attribute("value")->value();
        }

        for(xml_node <> *output = step->first_node("outputs")->first_node();
                output; output = output->next_sibling()) {
            s.outputs[output->first_attribute("name")->value()] = "";
        }

        _steps.push_back(s);
    }

    for(xml_node <> *output = root->first_node("outputs")->first_node();
            output; output = output->next_sibling()) {
        Parameter p;
        xml_attribute <> *attr;
        attr = output->first_attribute("type");
        if(!attr) {
            DBG_LOG << "Missing \"type\" in the workflow definition" << endl;
            throw __FILE__ " Error parsing workflow XML file";
        }
        string type = attr->value();
        if(type == string("file")) {
            p.para_type = DATA_FILE;
        } else if(type == string("bool")) {
            p.para_type = BOOL;
        } else if(type == string("float")) {
            p.para_type = FLOAT;
        } else if(type == string("bool")) {
            p.para_type = INT;
        } else if(type == string("text")) {
            p.para_type = TEXT;
        } else {
            p.para_type = UNKNOWN;
        }
        if(p.para_type == DATA_FILE) {
            attr = output->first_attribute("format");
            p.format = attr->value();
        }
        p.val = output->first_attribute("value")->value();
        _outputs[output->first_attribute("name")->value()] = p;
    }

    if(root->first_node("defines")) {
        for(xml_node <> *define = root->first_node("defines")->first_node();
                define; define = define->next_sibling()) {
            _defines[define->first_attribute("name")->value()] =
                define->first_attribute("value")->value();
        }
    }
    delete cstr;
    doc.clear();
}

void
Workflow::buildXml(const string &conf_file)
{
    xml_document <> doc;
    xml_node <> *root = doc.allocate_node(node_element, "workflow");
    doc.append_node(root);
    root->append_attribute(doc.allocate_attribute("id", _id.c_str()));
    root->append_attribute(doc.allocate_attribute("name", _name.c_str()));
    root->
    append_attribute(doc.allocate_attribute("version", _version.c_str()));
    xml_node <> *child = doc.allocate_node(node_element, "description");
    child->value(_desc.c_str());
    root->append_node(child);

    child = doc.allocate_node(node_element, "defines");
    for(map < string, string >::const_iterator it = _defines.begin();
            it != _defines.end(); it++) {
        xml_node <> *cc = doc.allocate_node(node_element, "param");
        cc->append_attribute(doc.allocate_attribute("name",
                             (*it).first.c_str()));
        cc->append_attribute(doc.allocate_attribute
                             ("value", (*it).second.c_str()));
        child->append_node(cc);
    }
    root->append_node(child);

    child = doc.allocate_node(node_element, "inputs");
    for(map < string, Parameter >::const_iterator it = _inputs.begin();
            it != _inputs.end(); it++) {
        xml_node <> *cc = doc.allocate_node(node_element, "param");
        cc->append_attribute(doc.allocate_attribute("name",
                             (*it).first.c_str()));
        cc->append_attribute(doc.allocate_attribute("type",
                             para_types[(*it).
                                        second.para_type].c_str
                             ()));
        if((*it).second.para_type == DATA_FILE)
            cc->append_attribute(doc.allocate_attribute("format",
                                 (*it).second.
                                 format.c_str()));
        child->append_node(cc);
    }
    root->append_node(child);

    child = doc.allocate_node(node_element, "steps");
    for(list < WorkflowStep >::const_iterator it = _steps.begin();
            it != _steps.end(); it++) {
        xml_node <> *cc = doc.allocate_node(node_element, "step");
        cc->append_attribute(doc.allocate_attribute("id", (*it).id.c_str()));
        cc->append_attribute(doc.allocate_attribute("type",
                             step_types[(*it).
                                        type].c_str
                             ()));
        cc->
        append_attribute(doc.allocate_attribute
                         ("plugin_id", (*it).plugin_id.c_str()));
        xml_node <> *input = doc.allocate_node(node_element, "inputs");
        for(map < string, string >::const_iterator itt = (*it).inputs.begin();
                itt != (*it).inputs.end(); itt++) {
            xml_node <> *ccc = doc.allocate_node(node_element, "param");
            ccc->append_attribute(doc.allocate_attribute("name",
                                  (*itt).first.
                                  c_str()));
            ccc->
            append_attribute(doc.allocate_attribute
                             ("value", (*itt).second.c_str()));
            input->append_node(ccc);
        }
        cc->append_node(input);

        xml_node <> *output = doc.allocate_node(node_element, "outputs");
        for(map < string, string >::const_iterator itt =
                    (*it).outputs.begin(); itt != (*it).outputs.end(); itt++) {
            xml_node <> *ccc = doc.allocate_node(node_element, "param");
            ccc->append_attribute(doc.allocate_attribute("name",
                                  (*itt).first.
                                  c_str()));
            ccc->
            append_attribute(doc.allocate_attribute
                             ("value", (*itt).second.c_str()));
            output->append_node(ccc);
        }
        cc->append_node(output);
        child->append_node(cc);
    }
    root->append_node(child);

    child = doc.allocate_node(node_element, "outputs");
    for(map < string, Parameter >::const_iterator it = _outputs.begin();
            it != _outputs.end(); it++) {
        xml_node <> *cc = doc.allocate_node(node_element, "param");
        cc->append_attribute(doc.allocate_attribute("name",
                             (*it).first.c_str()));
        cc->append_attribute(doc.allocate_attribute("type",
                             para_types[(*it).
                                        second.para_type].c_str
                             ()));
        if((*it).second.para_type == DATA_FILE)
            cc->append_attribute(doc.allocate_attribute("format",
                                 (*it).second.
                                 format.c_str()));
        cc->
        append_attribute(doc.allocate_attribute
                         ("value", (*it).second.val.c_str()));
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

void Workflow::addStep(WorkflowStep &ws)
{
    _steps.push_back(ws);
}

void
Workflow::addStep(int type, const string &id, const string &plugin_id, map< string, string >& inputs, map< string, string >& outputs)
{
    WorkflowStep w;
    w.type = type;
    w.id = id;
    w.plugin_id = plugin_id;
    for(map < string, string >::iterator input = inputs.begin();
            input != inputs.end(); input++) {
        w.inputs[(*input).first] = (*input).second;
    }
    for(map < string, string >::iterator output = outputs.begin();
            output != outputs.end(); output++) {
        w.outputs[(*output).first] = (*output).second;
    }
    _steps.push_back(w);
}

bool Workflow::delStep(const string &id)
{
    for(std::list< WorkflowStep >::iterator it = _steps.begin(); it != _steps.end(); it++) {
        if((*it).id == id) {
            _steps.erase(it);
            return true;
        }
    }

    return false;

}

void
Workflow::printInput()
{
    for(std::map < string, Parameter >::iterator input = _inputs.begin();
            input != _inputs.end(); input++)
        cout << "\tName=\"" << (*input).first << "\" Type=\"" <<
             para_types[(*input).second.para_type] << "\" Format=\"" <<
             (*input).second.format << "\"" << endl;
}

void
Workflow::printOutput()
{
    for(map < string, Parameter >::iterator output = _outputs.begin();
            output != _outputs.end(); output++)
        cout << "\tName=\"" << (*output).first << "\" Type=\"" <<
             para_types[(*output).second.para_type] << "\" Format=\"" << (*output).
             second.format << "\"" << " Value=\"" << (*output).
             second.val << "\"" << endl;
}

void
Workflow::addInput(const string &name, Parameter &p)
{
    _inputs[name] = p;
}

bool
Workflow::delInput(const string &name)
{
    if(_inputs.find(name) == _inputs.end()) {
        return false;
    }
    _inputs.erase(name);
    return true;
}

bool
Workflow::findInput(const string &name)
{
    if(_inputs.find(name) == _inputs.end()) {
        return false;
    }
    return true;
}

bool
Workflow::findOutput(const string &name)
{
    if(_outputs.find(name) == _outputs.end()) {
        return false;
    }
    return true;
}

bool
Workflow::findStepOutput(const string &id, const string &name)
{
    for(std::list< WorkflowStep >::iterator it = _steps.begin(); it != _steps.end(); it++) {
        if((*it).id == id) {
            if((*it).outputs.find(name) != (*it).outputs.end()) {
                return true;
            }
        }
    }

    return false;
}
void
Workflow::printStepOutput()
{
    for(list < WorkflowStep >::iterator step = _steps.begin();
            step != _steps.end(); step++) {
        cout << "\tStep " << (*step).
             id << ": type=\"" << step_types[(*step).type] << "\" plugin_id=\"" <<
             (*step).plugin_id << "\"" << endl;
        cout << "\t\tStep Outputs:" << endl;
        for(map < string, string >::iterator output =
                    (*step).outputs.begin(); output != (*step).outputs.end();
                output++) {
            cout << "\t\t\tName=\"" << (*step).id + "." + (*output).first << "\" " << endl;
        }
    }
}

void
Workflow::addOutput(const string &name, Parameter &p)
{
    _outputs[name] = p;
}

bool
Workflow::delOutput(const string &name)
{
    if(_outputs.find(name) == _outputs.end()) {
        return false;
    }
    _outputs.erase(name);
    return true;
}

Workflow::Workflow(const string &id, const string &name, const string &version, const string &description)
{
    _id = id;
    _name = name;
    _version = version;
    _desc = description;
}

Parameter Workflow::getParameter(const string &var)
{
    if(_inputs.find(var) != _inputs.end()) {
        return _inputs[var];
    }
    if(_outputs.find(var) != _outputs.end()) {
        return _outputs[var];
    }
    throw __FILE__ " Plugin::getParameter(): variable not found";
}

void
Workflow::print()
{
    cout << "ID: " << _id << endl;
    cout << "Name: " << _name << endl;
    cout << "Version: " << _version << endl;
    cout << "Description: " << _desc << endl;

    cout << "Inputs:" << endl;
    for(map < string, Parameter >::iterator input = _inputs.begin();
            input != _inputs.end(); input++)
        cout << "\tName=\"" << (*input).first << "\" Type=\"" <<
             para_types[(*input).second.para_type] << "\" Format=\"" <<
             (*input).second.format << "\"" << endl;
    cout << "Outputs:" << endl;
    for(map < string, Parameter >::iterator output = _outputs.begin();
            output != _outputs.end(); output++)
        cout << "\tName=\"" << (*output).first << "\" Type=\"" <<
             para_types[(*output).second.para_type] << "\" Format=\"" << (*output).
             second.format << "\"" << " Value=\"" << (*output).
             second.val << "\"" << endl;
    cout << "Defines:" << endl;
    for(map < string, string >::iterator define = _defines.begin();
            define != _defines.end(); define++) {
        cout << "\tName=\"" << (*define).first << "\" Value=\"" << (*define).second << "\"" << endl;
    }
    cout << "Steps:" << endl;
    for(list < WorkflowStep >::iterator step = _steps.begin();
            step != _steps.end(); step++) {
        cout << "\tStep " << (*step).
             id << ": type=\"" << step_types[(*step).type] << "\" plugin_id=\"" <<
             (*step).plugin_id << "\"" << endl;
        cout << "\t\tStep Inputs:" << endl;
        for(map < string, string >::iterator input =
                    (*step).inputs.begin(); input != (*step).inputs.end(); input++)
            cout << "\t\t\tName=\"" << (*input).first << "\" Value=\"" <<
                 (*input).second << "\" " << endl;
        cout << "\t\tStep Outputs:" << endl;
        for(map < string, string >::iterator output =
                    (*step).outputs.begin(); output != (*step).outputs.end();
                output++) {
            cout << "\t\t\tName=\"" << (*output).first << "\" " << endl;
        }
    }
}

string Workflow::getInfo()
{
    ostringstream oss;
    oss << "ID: \"" << _id << "\", Name: \"" << _name << "\", Version: \"" << _version << "\"";
    return oss.str();
}

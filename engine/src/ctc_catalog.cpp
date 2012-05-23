#include <iostream>
#include <fstream>
#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"
#include "ctc_catalog.h"
#include "ctc_util.h"
#include "ctc_plugin.h"
#include "ctc_workflow.h"
#include "ctc_logger.h"
#include "ctc_sysconfig.h"
#include <cstring>
#include <string>

using namespace rapidxml;
using namespace std;

Catalog *Catalog::_instance = NULL;

Catalog *Catalog::instance()
{
    if(_instance == NULL) {
        _instance = new Catalog();
    }

    return _instance;
}

int Catalog::openConfigFile(const string& root_dir, const string &conf_file)
{
    _root_dir = root_dir;
    _conf_file = conf_file;

    ifstream ifs(conf_file.c_str());
    if(!ifs) {
        ofstream ofs(conf_file.c_str());
        if(!ofs.is_open()) {
            throw __FILE__ " Catalog::openConfigFile: Cannot open catalog file for write";
        }
        // Default configuration
        ofs << "<suites>" << endl;
        ofs << "</suites>" << endl;

        ofs.close();
    } else {
        ifs.close();
    }

    xml_document<> doc;    // character type defaults to char
    char *cstr = getFileContent(conf_file);
    doc.parse<0>(cstr);
    for(xml_node<> *suite = doc.first_node()->first_node(); suite; suite = suite->next_sibling()) {
        Suite s;
        s.label = suite->first_attribute("label")->value();
        s.id = suite->first_attribute("id")->value();
        for(xml_node<> *node = suite->first_node(); node; node = node->next_sibling()) {
            s.members.push_back(node->first_attribute("id")->value());
            if(node->name() == string("plugin")) {
                _plugins[node->first_attribute("id")->value()] = node->first_attribute("file")->value();
            } else if(node->name() == string("workflow")) {
                _workflows[node->first_attribute("id")->value()] = node->first_attribute("file")->value();
            } else {
                DBG_LOG;
            }

        }
        _suites.push_back(s);
    }
    delete cstr;
    doc.clear();

    return 0;
}

int Catalog::updateConfigFile()
{
    // Write to a temp file
    string temp_file = _conf_file + ".tmp";
    // cout << "Building catalog: " << temp_file << endl;
    buildXml(temp_file);

    // If nothing wrong, move temp file to the config file
    if(moveFile(temp_file, _conf_file)) {
        throw __FILE__ " Error updating the system catalog";
    }
    return 0;
}

void Catalog::listSuites(const string &root_dir)
{
    for(vector <Suite>::iterator it = _suites.begin(); it != _suites.end(); it++) {
        cout << "Suite ID: " << (*it).id << ", Suite Label: " << (*it).label << endl;
        for(vector <string>::iterator itt = (*it).members.begin(); itt != (*it).members.end(); itt++) {
            string id = *itt;
            string conf_file;
            if(_plugins.find(id) != _plugins.end()) {
                conf_file = _plugins[(*itt)];
                if(conf_file.c_str()[0] != '/') {
                    conf_file = root_dir + conf_file;
                }
                Plugin p(conf_file);
                cout << "\tPlugin " << p.getInfo() << endl;
            } else if(_workflows.find(id) != _workflows.end()) {
                conf_file = _workflows[(*itt)];
                if(conf_file.c_str()[0] != '/') {
                    conf_file = root_dir + conf_file;
                }
                Workflow w(conf_file);
                cout << "\tWorkflow " << w.getInfo() << endl;
            }
        }
    }
}

bool Catalog::hasPlugin(const std::string &id)
{
    return (_plugins.find(id) != _plugins.end());
}

bool Catalog::hasWorkflow(const std::string &id)
{
    return (_workflows.find(id) != _workflows.end());
}

int Catalog::addSuiteMember(const string &suite, const string &id)
{
    int i;
    for(i = 0; i < _suites.size(); i++) {
        if(_suites[i].id == suite) {
            _suites[i].members.push_back(id);
            break;
        }
    }
    if(i == _suites.size()) {
        Suite s;
        s.id = suite;
        _suites.push_back(s);
        _suites[i].members.push_back(id);
    }

    return 0;
}

void Catalog::removeMember(const string& id) {
    for(int i = 0; i < _suites.size(); i++) {
        vector <string>::iterator it;
        for(it=_suites[i].members.begin(); it!=_suites[i].members.end(); it++) {
            if(*it == id) {
                _suites[i].members.erase(it);
                return;
            }
        }
    }
}

void Catalog::removeSuite(const string& id) {
    vector <Suite>::iterator it;
    vector <string> members;
    for(it=_suites.begin(); it!=_suites.end(); it++) {
        if(it->id == id) {
            members = it->members;
            break;
        }
    }

    for(int i=0; i<members.size(); i++) {
        if(_plugins.find(members[i]) != _plugins.end())
            removePlugin(members[i]);
        if(_workflows.find(members[i]) != _workflows.end())
            removeWorkflow(members[i]);
    }

    if(it != _suites.end()) {
        _suites.erase(it);
    }
}

int Catalog::addPlugin(const string &suite, const string &id, const string &conf_file)
{
    if(_plugins.find(id) != _plugins.end()) {
        DBG_LOG << " Plugin ID " << id << " already exists" << endl;
        return -1;
    }
    _plugins[id] = conf_file;
    addSuiteMember(suite, id);

    return 0;
}

void Catalog::removePlugin(const string& id) {
    string file = getPluginFile(id);
    _plugins.erase(id);
    removeMember(id);
    if(removeFile(file) != 0)
        throw __FILE__ " Error removing the plugin file";
}

int Catalog::addWorkflow(const string &suite, const string &id, const string &conf_file)
{
    if(_workflows.find(id) != _workflows.end()) {
        DBG_LOG << " Workflow ID " << id << " already exists" << endl;
        return -1;
    }
    _workflows[id] = conf_file;
    addSuiteMember(suite, id);

    return 0;
}

void Catalog::removeWorkflow(const string& id) {
    string file = getWorkflowFile(id);
    _workflows.erase(id);
    removeMember(id);
    if(removeFile(file) != 0)
        throw __FILE__ " Error removing the workflow file";
}

void Catalog::listPlugins()
{
    for(map <string, string>::iterator it = _plugins.begin(); it != _plugins.end(); it++) {
        cout << (*it).first << " " << (*it).second << endl;
    }
    cout << "listPlugins" << endl;
}

void Catalog::listWorkflows()
{
    for(map <string, string>::iterator it = _workflows.begin(); it != _workflows.end(); it++) {
        cout << (*it).first << " " << (*it).second << endl;
    }
}

void Catalog::getPluginIDs(vector <string>& ids)
{
    for(map <string, string>::iterator it = _plugins.begin(); it != _plugins.end(); it++) {
        ids.push_back((*it).first);
    }
}

void Catalog::getWorkflowIDs(vector<string>& ids)
{
    for(map <string, string>::iterator it = _workflows.begin(); it != _workflows.end(); it++) {
        ids.push_back((*it).first);
    }
}

string Catalog::getPluginFile(const string &id)
{
    if(_plugins.find(id) == _plugins.end()) {
        DBG_LOG << "Getting config file for plugin " << id << endl;
        throw __FILE__ " Catalog::getPluginFile(): invalid Plugin ID";
    }

    string config_file = _plugins[id];
    if(config_file.c_str()[0] != '/') {
        config_file = _root_dir + config_file;
    }
    return config_file;
}

string Catalog::getWorkflowFile(const string &id)
{
    if(_workflows.find(id) == _workflows.end()) {
        DBG_LOG << "Getting config file for workflow " << id << endl;
        throw __FILE__ " Catalog::getWorkflowFile(): invalid Workflow ID";
    }

    string config_file = _workflows[id];
    if(config_file.c_str()[0] != '/') {
        config_file = _root_dir + config_file;
    }
    return config_file;
}

void Catalog::buildXml(const string &conf_file)
{
    xml_document<> doc;
    xml_node<>* root = doc.allocate_node(node_element, "suites");
    doc.append_node(root);
    for(std::vector< Suite >::iterator suite = _suites.begin(); suite != _suites.end(); suite++) {
        xml_node<>* child = doc.allocate_node(node_element, "suite");
        child->append_attribute(doc.allocate_attribute("label", (*suite).label.c_str()));
        child->append_attribute(doc.allocate_attribute("id", (*suite).id.c_str()));
        bool plugin_suite = false;
        for(std::vector< std::string >::iterator member = (*suite).members.begin(); member != (*suite).members.end(); member++) {
            xml_node<>* cc;
            if(_plugins.find(*member) == _plugins.end()) {
                cc = doc.allocate_node(node_element, "workflow");
                cc->append_attribute(doc.allocate_attribute("id", (*member).c_str()));
                cc->append_attribute(doc.allocate_attribute("file", _workflows[(*member)].c_str()));
            } else {
                cc = doc.allocate_node(node_element, "plugin");
                cc->append_attribute(doc.allocate_attribute("id", (*member).c_str()));
                cc->append_attribute(doc.allocate_attribute("file", _plugins[(*member)].c_str()));
            }

            child->append_node(cc);

        }
        root->append_node(child);
    }
    ofstream ifs(conf_file.c_str());
    if(!ifs.is_open()) {
        throw __FILE__ " getFileContent(): Cannot open file";
    }
    ifs << doc;
    ifs.close();
    doc.clear();

}

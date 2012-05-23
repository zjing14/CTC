#ifndef __ctc_catalog_h__
#define __ctc_catalog_h__

#include <map>
#include <string>
#include <vector>

class Suite
{
public:
    std::string label;
    std::string id;
    std::vector <std::string> members; // stored ids for plugins and workflows of this suite
};

class Catalog
{
public:
    static Catalog *instance();
    int openConfigFile(const std::string& root_dir, const std::string &conf_file);
    int updateConfigFile();

    void listPlugins();
    void listWorkflows();
    void listSuites(const std::string &root_dir);

    int addPlugin(const std::string &suite, const std::string &id, const std::string &conf_file);
    void removePlugin(const std::string& id);
    int addWorkflow(const std::string &suite, const std::string &id, const std::string &conf_file);
    void removeWorkflow(const std::string& id);
    int addSuiteMember(const std::string &suite, const std::string &id);
    void removeMember(const std::string& id);
    void removeSuite(const std::string& id);
    
    bool hasPlugin(const std::string &id);
    bool hasWorkflow(const std::string &id);

    void getPluginIDs(std::vector<std::string>& ids);
    void getWorkflowIDs(std::vector<std::string>& ids);

    std::string getPluginFile(const std::string &id);
    std::string getWorkflowFile(const std::string &id);
    void buildXml(const std::string &conf_file);

protected:
    std::vector <Suite> _suites;
    std::map <std::string, std::string> _plugins;
    std::map <std::string, std::string> _workflows;

private:
    Catalog() {}
    Catalog(const Catalog &) {}
    Catalog &operator=(const Catalog &) {}

    static Catalog *_instance;
    std::string _conf_file;
    std::string _root_dir;
};

#endif

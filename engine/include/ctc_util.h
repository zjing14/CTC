#ifndef __ctc_util_h__
#define __ctc_util_h__

#include <map>
#include <string>

#define _MAX_PATH 2048

enum PARATYPE {
    INT,
    FLOAT,
    BOOL,
    TEXT,
    DATA_FILE,
    UNKNOWN
};

const std::string para_types [] = {
    "int",
    "float",
    "boolean",
    "text",
    "file",
    "unknown"
};

enum WorkflowStepType {
    PLUGIN,
    WORKFLOW,
    WS_UNKNOWN
};

const std::string step_types [] = {
    "plugin",
    "workflow",
    "unknown"
};

class Parameter
{
public:
    int para_type;
    std::string format;
    std::string val;
    std::string label;
    int int_min, int_max;
    float float_min, float_max;

    Parameter() {}
    Parameter(int type, const std::string &value, const std::string &format, const std::string &label);
};

std::string trimString(const std::string &str);

int createDir(const std::string &path);
int removeDir(const std::string &path);
std::string getTmpDir(const std::string &prefix);
std::string getCurrentDir();
int changeCurrentDir(const std::string &dir);

int removeFile(const std::string& file);
int moveFile(const std::string source, const std::string dest);
std::string getCurrentTime();
char *getFileContent(const std::string &file);

std::string resolveWord(std::string word, std::map <std::string, std::string>& exec_conf, std::map <std::string, std::string>& sys_conf);
std::string resolveConditional(std::string str, std::map <std::string, std::string>& exec_conf, std::map <std::string, std::string>& sys_conf);
std::string resolveStream(std::string str, std::map <std::string, std::string>& exec_conf, std::map <std::string, std::string>& sys_conf);

#endif

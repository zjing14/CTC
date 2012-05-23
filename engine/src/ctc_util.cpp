#include "ctc_util.h"
#include "ctc_logger.h"
#include "ctc_sysconfig.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
using namespace std;

Parameter::Parameter(int type, const string &value, const string &format, const string &label)
{
    this->para_type = type;
    this->val =  value;
    this->format = format;
    this->label = label;
}

string trimString(const string &str)
{
    string space(" \t\n\r");
    size_t pos = str.find_first_not_of(space);
    size_t pos1 = str.find_last_not_of(space);
    return str.substr(pos, pos1 - pos + 1);
}

// The memory pointer allocated by this function needs to be released by the caller.
char *getFileContent(const string &file)
{
    ifstream ifs(file.c_str());
    if(!ifs.is_open()) {
        DBG_LOG << "Opening file: " << file << endl;
        throw __FILE__ " getFileContent(): Cannot open file";
    }
    ifs.seekg(0, ios::end);
    int size = ifs.tellg();
    char *memblock = new char [size + 1];
    ifs.seekg(0, ios::beg);
    ifs.read(memblock, size);
    ifs.close();
    memblock[size] = '\0';
    return memblock;
}

int createDir(const string &path)
{
    int rval;
    struct stat stat_buf;
    mode_t mode = 0x1fd; // mode 775
    if((rval = mkdir(path.c_str(), mode)) == -1 && errno == EEXIST) {
        stat(path.c_str(), &stat_buf);
        return S_ISDIR(stat_buf.st_mode);
    }
    return rval == 0;
}

int removeDir(const string &path)
{
    if(rmdir(path.c_str())) {
        return -1;
    }
    return 0;
}

// Require non-empty prefix
string getTmpDir(const string &prefix)
{
    string temp_name = prefix;
    if(prefix.c_str()[prefix.length() - 1] != '/') {
        temp_name += "/";
    }

    temp_name += "XXXXXXXXXX";
    char dir_name[_MAX_PATH];
    snprintf(dir_name, temp_name.length(), "%s", temp_name.c_str());

    if(mkdtemp(dir_name) == NULL) {
        throw __FILE__ " getTmpDir(): Error creating temporary dir";
    }

    return dir_name;
}

string getCurrentDir()
{
    char org_dir[_MAX_PATH];
    if(getcwd(org_dir, sizeof(org_dir)) == NULL) {
        throw __FILE__ " getCurrentDir(): Error getting current dir";
    }
    return org_dir;
}

int changeCurrentDir(const string &dir)
{
    if(chdir(dir.c_str())) {
        return -1;
    } else {
        return 0;
    }
}

int moveFile(const std::string source, const std::string dest)
{
    if(rename(source.c_str(), dest.c_str())) {
        return -1;
    } else {
        return 0;
    }
}

int removeFile(const std::string& file) {
    return remove(file.c_str());
}

string getCurrentTime()
{
    time_t t;
    time(&t);

    string asc_time = ctime(&t);

    return asc_time.substr(0, asc_time.length() - 1);
}

string resolveWord(string word, map <string, string>& exec_conf, map<string, string>& sys_conf)
{
    string result;

    // trim word
    word.erase(remove_if(word.begin(), word.end(), (int( *)(int))isspace), word.end());
    // '"' only allowed in the beginning and end of a word
    if(word.c_str()[0] == '"') {
        size_t pos = word.find('"', 1);
        if(pos == string::npos) {
            DBG_LOG << "Error parsing variable " << word << endl;
            throw __FILE__ " Error resolving words";
        }
        return word.substr(1, pos - 1);
    }

    size_t current_pos = 0;
    while(true) {
        size_t pos = word.find('$', current_pos);
        result += word.substr(current_pos, pos - current_pos);

        if(pos == string::npos) {
            break;
        }

        string var_name;
        size_t pos1;
        if(word.c_str()[pos + 1] == '{') {
            pos1 = word.find('}', pos + 1);
            if(pos1 == string::npos) {
                DBG_LOG << "Error parsing variable " << word << endl;
                throw __FILE__ " Error resolving words";
            }

            var_name = word.substr(pos + 2, pos1 - pos - 2);
            pos1++;
        } else {
            for(pos1 = pos + 1; pos1 < word.length(); pos1++) {
                char ch = word.c_str()[pos1];
                if(ch == '/' || ch == '\\' || ch == '$') {
                    break;
                }
            }
            var_name = word.substr(pos + 1, pos1 - pos - 1);
        }

        if(exec_conf.find(var_name) != exec_conf.end()) {
            result += exec_conf[var_name];
        } else if(sys_conf.find(var_name) != sys_conf.end()) {
            result += sys_conf[var_name];
        } else {
            char *env = getenv(var_name.c_str());
            if(env == NULL) {
                DBG_LOG << "Error resolving variable: " << var_name << endl;
                throw __FILE__ " Error resolving variables";
            }
            result += env;
        }
        current_pos = pos1;
    }

    return result;
}

string resolveConditional(string str, map <string, string>& exec_conf, map<string, string>& sys_conf)
{
    size_t pos = str.find('?');
    if(pos == string::npos) {
        DBG_LOG << "Missing \'?\' in the conditional" << endl;
        throw __FILE__ " Error parsing conditional";
    }

    bool has_operator = true;
    string condition = str.substr(0, pos);
    size_t pos1 = condition.find("==");
    if(pos1 == string::npos) {
        pos1 = condition.find("!=");
        if(pos1 == string::npos) {
            has_operator = false;
        }
    }
    string value = str.substr(pos + 1);

    bool eval = false;

    if(has_operator) {
        string op = condition.substr(pos1, 2);
        string opn1 = condition.substr(0, pos1);
        string opn2 = condition.substr(pos1 + 2);

        opn1 = resolveWord(opn1, exec_conf, sys_conf);
        opn2 = resolveWord(opn2, exec_conf, sys_conf);
        if(op == "==" && opn1 == opn2 || op == "!=" && opn1 != opn2) {
            eval = true;
        }
    } else {
        if(resolveWord(condition, exec_conf, sys_conf) == "true") {
            eval = true;
        }
    }

    pos1 = value.find(":");
    if(pos1 != string::npos) {
        if(eval) {
            return resolveStream(value.substr(0, pos1 - 1), exec_conf, sys_conf);
        } else {
            return resolveWord(value.substr(pos1 + 1), exec_conf, sys_conf);
        }
    } else {
        if(eval) {
            return resolveStream(value.substr(0), exec_conf, sys_conf);
        } else {
            return "";
        }
    }
}

string resolveStream(string str, map <string, string>& exec_conf, map<string, string>& sys_conf)
{
    string result;

    string white(" \t\n\r");
    size_t current_pos = 0;
    while(true) {
        if(white.find(str.c_str()[current_pos]) != string::npos) {
            result += " ";
        }

        size_t pos, pos1;
        pos = str.find_first_not_of(white, current_pos);
        if(pos == string::npos) {
            break;
        }

        bool conditional = false;
        if(str.c_str()[pos] == '[') {
            conditional = true;
            pos1 = str.find(']', pos + 1);
            if(pos1 == string::npos) {
                DBG_LOG << "Missing \']\' in command: " << str << endl;
                throw __FILE__ " error parsing command";
            }
            result += resolveConditional(str.substr(pos + 1, pos1 - pos - 1), exec_conf, sys_conf);
            current_pos = pos1 + 1;
        } else if(str.c_str()[pos] == '"') {
            pos1 = str.find('"', pos + 1);
            if(pos1 == string::npos) {
                DBG_LOG << "Unpaired \" in command: " << str << endl;
                throw __FILE__ " error parsing command";
            }
            result += str.substr(pos, pos1 - pos + 1);
            current_pos = pos1 + 1;
        } else {
            pos1 = str.find_first_of(white, pos);
            result += resolveWord(str.substr(pos, pos1 - pos), exec_conf, sys_conf);
            current_pos = pos1;
        }
    }

    return result;
}

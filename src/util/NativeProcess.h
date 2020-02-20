/* @@@LICENSE
 *
 * Copyright (c) 2020 LG Electronics, Inc.
 *
 * Confidential computer software. Valid license from LG required for
 * possession, use or copying. Consistent with FAR 12.211 and 12.212,
 * Commercial Computer Software, Computer Software Documentation, and
 * Technical Data for Commercial Items are licensed to the U.S. Government
 * under vendor's standard commercial license.
 *
 * LICENSE@@@
 */

#ifndef UTIL_NATIVEPROCESS_H_
#define UTIL_NATIVEPROCESS_H_

#include <iostream>
#include <vector>
#include <map>
#include <glib.h>

using namespace std;

class NativeProcess {
public:
    NativeProcess();
    virtual ~NativeProcess();

    void setWorkingDirectory(const string& directory)
    {
        m_workingDirectory = directory;
    }

    void setCommand(const string& command)
    {
        m_command = command;
    }
    const string& getCommand()
    {
        return m_command;
    }

    void addArgument(const string& argument);
    void addArgument(const string& option, const string& value);
    void addEnv(map<string, string>& environments);
    void addEnv(const string& variable, const string& value);

    const pid_t getPid() const
    {
        return m_pid;
    }
    void setPid(pid_t pid)
    {
        m_pid = pid;
    }

    void openLogfile(const string& logfile);
    void closeLogfile();

    bool run();
    bool term();
    bool kill();

    void track()
    {
        m_isTracked = true;
    }

    bool isTracked()
    {
        return m_isTracked;
    }

private:
    static const string CLASS_NAME;
    static const int MAX_ARGS = 16;
    static const int MAX_ENVP = 64;

    static void convertEnvToStr(map<string, string>& src, vector<string>& dest);
    static void prepareSpawn(gpointer user_data);

    string m_workingDirectory;
    string m_command;

    vector<string> m_arguments;
    map<string, string> m_environments;

    pid_t m_pid;
    gint m_logfile;

    bool m_isTracked;

};

#endif /* UTIL_NATIVEPROCESS_H_ */

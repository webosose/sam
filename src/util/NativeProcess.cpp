/* @@@LICENSE
 *
 * Copyright (c) 2020-2023 LG Electronics, Inc.
 *
 * Confidential computer software. Valid license from LG required for
 * possession, use or copying. Consistent with FAR 12.211 and 12.212,
 * Commercial Computer Software, Computer Software Documentation, and
 * Technical Data for Commercial Items are licensed to the U.S. Government
 * under vendor's standard commercial license.
 *
 * LICENSE@@@
 */

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "util/NativeProcess.h"
#include "util/Logger.h"

const string NativeProcess::CLASS_NAME = "NativeProcess";

void NativeProcess::convertEnvToStr(map<string, string>& src, vector<string>& dest)
{
    for (auto it = src.begin(); it != src.end(); ++it) {
        dest.push_back(it->first + "=" + it->second);
    }
}

void NativeProcess::prepareSpawn(gpointer user_data)
{
    // This function is called in child context.
    // setpgid is needed to kill all processes which are created by application at once
    int result = setpgid(getpid(), 0);
    if (result == -1) {
        Logger::error(CLASS_NAME, __FUNCTION__, strerror(errno));
    }
}

NativeProcess::NativeProcess()
    : m_workingDirectory("/"),
      m_command(""),
      m_pid(-1),
      m_stdFd(-1),
      m_isTracked(false)
{

}

NativeProcess::~NativeProcess()
{

}

void NativeProcess::addArgument(const string& argument)
{
    m_arguments.push_back(argument);
}

void NativeProcess::addArgument(const string& option, const string& value)
{
    m_arguments.push_back(option);
    m_arguments.push_back(value);
}

void NativeProcess::addEnv(map<string, string>& environments)
{
    for (auto it = environments.begin(); it != environments.end(); ++it) {
        m_environments[it->first] = it->second;
    }
}

void NativeProcess::addEnv(const string& variable, const string& value)
{
    m_environments[variable] = value;
}

void NativeProcess::openStdFile(const string& stdFile)
{
    closeStdFd();
    m_stdFile = stdFile;
    m_stdFd = ::open(stdFile.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);
}

void NativeProcess::closeStdFd()
{
    if (m_stdFd >= 0)
        close(m_stdFd);
}

bool NativeProcess::run()
{
    const char* argv[MAX_ARGS] = { 0, };
    const char* envp[MAX_ENVP] = { 0, };
    string params = "";
    unsigned int index = 0;

    GError* gerr = NULL;
    argv[0] = m_command.c_str();
    index = 1;
    for (auto it = m_arguments.begin(); it != m_arguments.end() && index < MAX_ARGS; ++it) {
        params += *it + " ";
        argv[index++] = it->c_str();
    }

    vector<string> finalEnvironments;
    convertEnvToStr(m_environments, finalEnvironments);
    index = 0;
    for (auto it = finalEnvironments.begin(); it != finalEnvironments.end() && index < MAX_ENVP; ++it) {
        envp[index++] = (const char*)it->c_str();
    }

    Logger::info(CLASS_NAME, __FUNCTION__, m_command, params);
    gboolean result = g_spawn_async_with_fds(
        m_workingDirectory.c_str(),
        const_cast<char**>(argv),
        const_cast<char**>(envp),
        G_SPAWN_DO_NOT_REAP_CHILD,
        prepareSpawn,
        this,
        &m_pid,
        -1,
        m_stdFd,
        m_stdFd,
        &gerr
    );
    if (gerr) {
        Logger::error(CLASS_NAME, __FUNCTION__, gerr->message);
        g_error_free(gerr);
        gerr = NULL;
        return false;
    }
    if (!result || m_pid <= 0) {
        Logger::error(CLASS_NAME, __FUNCTION__, "Failed to folk child process");
        return false;
    }
    return true;
}

bool NativeProcess::term()
{
    if (m_pid <= 0) {
        Logger::error(CLASS_NAME, __FUNCTION__, "Process is not running");
        return false;
    }
    int result = killpg(m_pid, SIGTERM);
    if (result == -1) {
        Logger::error(CLASS_NAME, __FUNCTION__, strerror(errno));
        return false;
    }
    return true;
}

bool NativeProcess::kill()
{
    if (m_pid <= 0) {
        Logger::error(CLASS_NAME, __FUNCTION__, "Process is not running");
        return false;
    }
    int result = killpg(m_pid, SIGKILL);
    if (result == -1) {
        Logger::error(CLASS_NAME, __FUNCTION__, strerror(errno));
        return false;
    }
    return true;
}

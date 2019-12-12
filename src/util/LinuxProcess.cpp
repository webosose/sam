/* @@@LICENSE
 *
 * Copyright (c) 2019 LG Electronics, Inc.
 *
 * Confidential computer software. Valid license from LG required for
 * possession, use or copying. Consistent with FAR 12.211 and 12.212,
 * Commercial Computer Software, Computer Software Documentation, and
 * Technical Data for Commercial Items are licensed to the U.S. Government
 * under vendor's standard commercial license.
 *
 * LICENSE@@@
 */

#include <signal.h>
#include <glib.h>
#include <proc/readproc.h>
#include <stdlib.h>

#include "util/LinuxProcess.h"

const string LinuxProcess::CLASS_NAME = "LinuxProcess";

bool LinuxProcess::sendSigTerm(const string& pid)
{
    if (pid.empty()) {
        Logger::error(CLASS_NAME, __FUNCTION__, "pid is empty");
        return false;
    }

    PidVector allPids = LinuxProcess::findChildPids(pid);
    if (allPids.empty()) {
        Logger::error(CLASS_NAME, __FUNCTION__, "empty_pids");
        return false;
    }

    if (!LinuxProcess::killProcesses(allPids, SIGTERM)) {
        Logger::error(CLASS_NAME, __FUNCTION__, "seding_signal_error");
        return false;
    }
    return true;
}

bool LinuxProcess::sendSigKill(const string& pid)
{
    if (pid.empty()) {
        Logger::error(CLASS_NAME, __FUNCTION__, "pid is empty");
        return false;
    }

    PidVector allPids = LinuxProcess::findChildPids(pid);
    if (allPids.empty()) {
        Logger::error(CLASS_NAME, __FUNCTION__, "empty_pids");
        return false;
    }

    if (!LinuxProcess::killProcesses(allPids, SIGKILL)) {
        Logger::error(CLASS_NAME, __FUNCTION__, "seding_signal_error");
        return false;
    }
    return true;
}

string LinuxProcess::convertPidsToString(const PidVector& pids)
{
    string result;
    string delim;
    for (pid_t pid : pids) {
        result += delim;
        result += to_string(pid);
        delim = " ";
    }
    return result;
}

bool LinuxProcess::killProcesses(const PidVector& pids, int sig)
{
    auto it = pids.begin();
    if (it == pids.end())
        return true;

    // first process is parent process, killing child processes later can fail if parent itself terminates them
    bool success = kill(*it, sig) == 0;
    while (++it != pids.end()) {
        kill(*it, sig);
    }
    return success;
}

pid_t LinuxProcess::forkProcess(const char **argv, const char **envp)
{
    //TODO : Set child's working path
    GPid pid = -1;
    GError* gerr = NULL;
    GSpawnFlags flags = (GSpawnFlags) (G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_DO_NOT_REAP_CHILD);

    if (true) {
        string cmd = "";
        // This is debug purpose
        for (int i = 0; i < 10; i++) {
            if (argv[i] == NULL) break;
            cmd += string(argv[i]) + " ";
        }
        Logger::error(CLASS_NAME, __FUNCTION__, cmd);
    }

    gboolean result = g_spawn_async_with_pipes(
        NULL,
        const_cast<char**>(argv),  // cmd arguments
        const_cast<char**>(envp),  // environment variables
        flags,
        NULL,
        NULL,
        &pid,
        NULL,
        NULL,
        NULL,
        &gerr
    );
    if (gerr) {
        Logger::error(CLASS_NAME, __FUNCTION__, Logger::format("Failed to folk: pid(%d) errorText(%s)", pid, gerr->message));
        g_error_free(gerr);
        gerr = NULL;
        return -1;
    }
    if (!result) {
        Logger::error(CLASS_NAME, __FUNCTION__, Logger::format("Failed to folk: pid: %d", pid));
        return -1;
    }

    return pid;
}

PidVector LinuxProcess::findChildPids(const string& pid)
{
    PidVector pids;
    pids.push_back((pid_t) atol(pid.c_str()));

    proc_t **proctab = readproctab(PROC_FILLSTAT);
    if (!proctab) {
        Logger::error(CLASS_NAME, __FUNCTION__, "readproctab_error", "failed to read proctab");
        return pids;
    }

    size_t idx = 0;
    while (idx != pids.size()) {
        for (proc_t **proc = proctab; *proc; ++proc) {
            pid_t tid = (*proc)->tid;
            pid_t ppid = (*proc)->ppid;
            if (ppid == pids[idx]) {
                pids.push_back(tid);
            }
        }
        ++idx;
    }

    for (proc_t **proc = proctab; *proc; ++proc) {
        free(*proc);
    }

    free(proctab);
    return pids;
}
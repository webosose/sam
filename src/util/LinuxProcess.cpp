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

#include <util/LinuxProcess.h>
#include <signal.h>
#include <glib.h>
#include "util/Logging.h"
#include <proc/readproc.h>
#include <stdlib.h>

string LinuxProcess::PidsToString(const PidVector& pids)
{
    string result;
    string delim;
    for (pid_t pid : pids) {
        result += delim;
        result += std::to_string(pid);
        delim = " ";
    }
    return result;
}

bool LinuxProcess::kill_processes(const PidVector& pids, int sig)
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

pid_t LinuxProcess::fork_process(const char **argv, const char **envp)
{
    //TODO : Set child's working path
    GPid pid = -1;
    GError* gerr = NULL;
    GSpawnFlags flags = (GSpawnFlags) (G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_DO_NOT_REAP_CHILD);

    gboolean result = g_spawn_async_with_pipes(NULL,
                                               const_cast<char**>(argv),  // cmd arguments
                                               const_cast<char**>(envp),  // environment variables
                                               flags,
                                               NULL,
                                               NULL, &pid,
                                               NULL,
                                               NULL,
                                               NULL,
                                               &gerr);
    if (gerr) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "fork_fail"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "returned_pid: %d, err_text: %s", (int )pid, gerr->message);
        g_error_free(gerr);
        gerr = NULL;
        return -1;
    }

    if (!result) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "return_false_from_gspawn"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "returned_pid: %d", pid);
        return -1;
    }

    return pid;
}

PidVector LinuxProcess::FindChildPids(const std::string& pid)
{
    PidVector pids;
    pids.push_back((pid_t) std::atol(pid.c_str()));

    proc_t **proctab = readproctab(PROC_FILLSTAT);
    if (!proctab) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 1,
                  PMLOGKS(LOG_KEY_REASON, "readproctab_error"),
                  "failed to read proctab");
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

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

#ifndef UTIL_LINUXPROCESS_H_
#define UTIL_LINUXPROCESS_H_

#include <iostream>
#include <vector>
#include <fcntl.h>
#include "util/Logger.h"

using namespace std;

typedef std::vector<pid_t> PidVector;

class LinuxProcess {
public:
    LinuxProcess() {}
    virtual ~LinuxProcess() {}

    static string convertPidsToString(const PidVector& pids);
    static bool killProcesses(const PidVector& pids, int sig);
    static pid_t forkProcess(const char **argv, const char **envp);
    static PidVector findChildPids(const std::string& pid);

private:
    static const string CLASS_NAME;
};

#endif /* UTIL_LINUXPROCESS_H_ */

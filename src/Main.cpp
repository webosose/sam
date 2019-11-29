// Copyright (c) 2012-2019 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include <signal.h>
#include <stdlib.h>

#include <sstream>
#include <fstream>
#include <gio/gio.h>

#include "MainDaemon.h"
#include "util/Logger.h"
#include "util/File.h"

static const char* CLASS_NAME = "Main";

void signal_handler(int signal, siginfo_t *siginfo, void *context)
{
    string buf;
    stringstream stream;
    string sender_pid, sender_cmdline;
    int si_code;

    // abstract sender_pid from struct
    stream << siginfo->si_pid;
    sender_pid = stream.str();
    sender_cmdline = "/proc/" + sender_pid + "/cmdline";
    si_code = siginfo->si_code;

    Logger::warning(CLASS_NAME, __FUNCTION__, Logger::format("signal(%d) si_code(%d) sender_pid(%s) sender_cmdline(%s)", signal, si_code, sender_pid.c_str(), sender_cmdline.c_str()));
    buf = File::readFile(sender_cmdline.c_str());
    if (buf.empty()) {
        Logger::warning(CLASS_NAME, __FUNCTION__, Logger::format("signal(%d) si_code(%d) si_pid(%d), si_uid(%d)", signal, si_code, siginfo->si_pid, siginfo->si_uid));
        goto Done;
    }

    Logger::warning(CLASS_NAME, __FUNCTION__, Logger::format("signal(%d) si_code(%d) sender(%s) si_pid(%d), si_uid(%d)", signal, si_code, buf.c_str(), siginfo->si_pid, siginfo->si_uid));

Done:
    if (signal == SIGHUP || signal == SIGINT || signal == SIGPIPE) {
        Logger::warning(CLASS_NAME, __FUNCTION__, "Ignore received signal");
        return;
    } else {
        Logger::warning(CLASS_NAME, __FUNCTION__, "Try to terminate SAM process");
    }

    MainDaemon::getInstance().stop();
}

int main(int argc, char **argv)
{
    Logger::info(CLASS_NAME, __FUNCTION__, "Start SAM process");

    // tracking sender if we get some signal
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_sigaction = signal_handler;
    act.sa_flags = SA_SIGINFO;

    // we will ignore this signal
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGPIPE, &act, NULL);

    // we don't change the default action
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGABRT, &act, NULL);
    sigaction(SIGFPE, &act, NULL);

    MainDaemon::getInstance().initialize();
    MainDaemon::getInstance().start();
    MainDaemon::getInstance().finalize();

    return EXIT_SUCCESS;
}

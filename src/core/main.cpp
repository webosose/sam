// Copyright (c) 2012-2018 LG Electronics, Inc.
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

#include "core/base/logging.h"
#include "core/base/utils.h"
#include "core/main_service.h"

MainService service;

void signal_handler(int signal, siginfo_t *siginfo, void *context)
{
    std::string buf;
    std::stringstream stream;
    std::string sender_pid, sender_cmdline;
    int si_code;

    // abstract sender_pid from struct
    stream << siginfo->si_pid;
    sender_pid = stream.str();
    sender_cmdline = "/proc/" + sender_pid + "/cmdline";
    si_code = siginfo->si_code;

    LOG_WARNING(MSGID_RECEIVED_SYS_SIGNAL, 4, PMLOGKFV("signal_no", "%d", signal), PMLOGKFV("signal_code", "%d", si_code), PMLOGKS("sender_pid", sender_pid.c_str()),
            PMLOGKS("sender_cmdline", sender_cmdline.c_str()), "received signal");

    buf = read_file(sender_cmdline.c_str());
    if (buf.empty()) {
        LOG_WARNING(MSGID_RECEIVED_SYS_SIGNAL, 4, PMLOGKFV("signal_no", "%d", signal), PMLOGKFV("signal_code", "%d", si_code), PMLOGKFV("sender_pid", "%d", (int)siginfo->si_pid),
                PMLOGKFV("sender_uid", "%d", (int)siginfo->si_uid), "fopen fail");
        goto Done;
    }

    LOG_WARNING(MSGID_RECEIVED_SYS_SIGNAL, 5, PMLOGKFV("signal_no", "%d", signal), PMLOGKFV("signal_code", "%d", si_code), PMLOGKS("sender_name", buf.c_str()),
            PMLOGKFV("sender_pid", "%d", (int)siginfo->si_pid), PMLOGKFV("sender_uid", "%d", (int)siginfo->si_uid), "signal information");

    Done: if (signal == SIGHUP || signal == SIGINT || signal == SIGPIPE) {
        LOG_WARNING(MSGID_RECEIVED_SYS_SIGNAL, 1, PMLOGKS("action", "ignore"), "just ignore");
        return;
    } else {
        LOG_WARNING(MSGID_RECEIVED_SYS_SIGNAL, 1, PMLOGKS("action", "terminate"), "sam process is now terminating");
    }

    service.stop();
}

int main(int argc, char **argv)
{
    LOG_INFO(MSGID_SAM_LOADING_SEQ, 1, PMLOGKS("status", "starting"), "");

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

    service.start();

    return EXIT_SUCCESS;
}

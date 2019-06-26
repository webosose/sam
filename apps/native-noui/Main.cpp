// Copyright (c) 2019 LG Electronics, Inc.
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

#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

using namespace std;

void writeMsg(const string& msg, const string& filename)
{
    cout << msg << endl;
    string cmd = "echo " + msg + " >> " + filename;
    system(cmd.c_str());
}

int main(int argc, char **argv)
{
    string msg = "";
    for (int i = 0; i < argc; ++i) {
        msg += std::to_string(i) + "_" + argv[i] + " ";
    }

    int pid = getpid();
    string filename = "/tmp/" + to_string(pid) + "_.log";
    while(true) {
        writeMsg(msg, filename);
        sleep(1);
    }
    return 0;
}

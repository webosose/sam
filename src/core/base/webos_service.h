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

#ifndef WEBOS_SERVICE_BASE_H
#define WEBOS_SERVICE_BASE_H

#include <glib.h>

class WebOSService {
public:
    virtual ~WebOSService()
    {
    }

    void start();
    void stop();
    void create_instance();
    void destroy_instance();
    void run_thread();
    void stop_thread();

protected:
    virtual bool initialize() = 0;
    virtual bool terminate() = 0;

    GMainLoop* main_loop()
    {
        return m_main_loop;
    }

private:
    void init();
    void run();
    void cleanup();

    GMainLoop* m_main_loop;
};

#endif

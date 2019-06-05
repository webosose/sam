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

#include <base/webos_service.h>

void WebOSService::start()
{
    init();
    run();
    cleanup();
}

void WebOSService::stop()
{
    if (m_main_loop)
        g_main_loop_quit(m_main_loop);
}

void WebOSService::createInstance()
{
    init();
}

void WebOSService::destroyInstance()
{
    cleanup();
}

void WebOSService::runThread()
{
    initialize();
}

void WebOSService::stopThread()
{
    terminate();
}

void WebOSService::init()
{
    m_main_loop = g_main_loop_new(NULL, FALSE);
    initialize();
}

void WebOSService::run()
{
    g_main_loop_run(m_main_loop);
}

void WebOSService::cleanup()
{
    terminate();
    if (m_main_loop) {
        g_main_loop_unref(m_main_loop);
    }
}

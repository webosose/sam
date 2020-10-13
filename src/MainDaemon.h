// Copyright (c) 2012-2020 LG Electronics, Inc.
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

#ifndef MAIN_DAEMON_H
#define MAIN_DAEMON_H

#include <luna-service2/lunaservice.h>

#include "base/LunaTask.h"
#include "interface/ISingleton.h"
#include "interface/IClassName.h"

class MainDaemon : public ISingleton<MainDaemon>,
                   public IClassName {
friend class ISingleton<MainDaemon>;
public:
    virtual ~MainDaemon();

    void initialize();
    void finalize();

    void start();
    void stop();

private:
    MainDaemon();

    void onGetBootStatus(const JValue& subscriptionPayload);
    void onGetConfigs(const JValue& subscriptionPayload);

    void checkPreconditions();

    bool m_isCBDGenerated;
    bool m_isConfigsReceived;

    GMainLoop *m_mainLoop;

};

#endif

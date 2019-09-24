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


#ifndef MANAGER_POLICYMANAGER_H_
#define MANAGER_POLICYMANAGER_H_

#include <iostream>

#include "base/AppDescription.h"
#include "base/AppDescriptionList.h"
#include "base/LaunchPoint.h"
#include "base/LaunchPointList.h"
#include "base/LunaTask.h"
#include "base/LunaTaskList.h"
#include "base/RunningApp.h"
#include "base/RunningAppList.h"
#include "interface/ISingleton.h"
#include "interface/IClassName.h"

using namespace std;

class PolicyManager : public ISingleton<PolicyManager>,
                      public IClassName {
friend class ISingleton<PolicyManager> ;
public:
    virtual ~PolicyManager();

    void launch(LunaTaskPtr lunaTask);
    void removeLaunchPoint(LunaTaskPtr lunaTask);

private:
    void checkExecutionLock(LunaTaskPtr item);

    PolicyManager();
};

#endif /* MANAGER_POLICYMANAGER_H_ */

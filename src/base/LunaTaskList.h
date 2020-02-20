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

#ifndef BASE_LUNATASKLIST_H_
#define BASE_LUNATASKLIST_H_

#include <iostream>
#include <list>

#include "interface/ISingleton.h"
#include "LunaTask.h"

using namespace std;

class LunaTaskList : public ISingleton<LunaTaskList> {
friend class ISingleton<LunaTaskList>;
public:
    virtual ~LunaTaskList();

    LunaTaskPtr create();

    LunaTaskPtr getByKindAndId(const char* kind, const string& appId);
    LunaTaskPtr getByInstanceId(const string& instanceId);
    LunaTaskPtr getByToken(const LSMessageToken& token);

    bool add(LunaTaskPtr lunaTask);
    bool removeAfterReply(LunaTaskPtr lunaTask, const int errorCode, const string& errorText);
    bool removeAfterReply(LunaTaskPtr lunaTask);

    void toJson(JValue& array);

private:
    LunaTaskList();

    list<LunaTaskPtr> m_list;
};

#endif /* BASE_LUNATASKLIST_H_ */

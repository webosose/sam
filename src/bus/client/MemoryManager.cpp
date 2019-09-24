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

#include <bus/client/MemoryManager.h>

MemoryManager::MemoryManager()
    : AbsLunaClient("com.webos.service.memorymanager")
{
    setClassName("MemoryManager");
}

MemoryManager::~MemoryManager()
{
    // TODO Auto-generated destructor stub
}

void MemoryManager::onInitialze()
{

}

void MemoryManager::onServerStatusChanged(bool isConnected)
{
}

void MemoryManager::requireMemory(LunaTaskPtr lunaTask)
{
    if (!lunaTask->getAPICallback().empty()) {
        lunaTask->getAPICallback()(lunaTask);
    }
}

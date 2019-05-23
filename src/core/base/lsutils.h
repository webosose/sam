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

#ifndef _LSUTILS_H_
#define _LSUTILS_H_

#include <string>
#include <vector>

#include <luna-service2/lunaservice.h>

class LSErrorSafe: public LSError {
public:
    LSErrorSafe()
    {
        LSErrorInit(this);
    }
    ~LSErrorSafe()
    {
        LSErrorFree(this);
    }
};

std::string GetCallerFromMessage(LSMessage* message);
std::string GetCallerID(const std::string& caller);
std::string GetCallerPID(const std::string& caller);
std::string GetCategoryFromMessage(LSMessage* message);
std::string GetMethodFromMessage(LSMessage* message);

#endif

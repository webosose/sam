// Copyright (c) 2017-2020 LG Electronics, Inc.
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

#ifndef BUS_SERVICE_SCHEMACHECKER_H_
#define BUS_SERVICE_SCHEMACHECKER_H_

#include <iostream>
#include <map>
#include <luna-service2/lunaservice.hpp>
#include <pbnjson.hpp>

#include "interface/ISingleton.h"

using namespace std;
using namespace LS;
using namespace pbnjson;

class SchemaChecker : public ISingleton<SchemaChecker> {
friend class ISingleton<SchemaChecker>;
public:
    virtual ~SchemaChecker();

    JValue getRequestPayloadWithSchema(Message& request);
    string getAPISchemaFilePath(const string& method);

private:
    SchemaChecker();

    map<string, string> m_APISchemaFiles;
};

#endif /* BUS_SERVICE_SCHEMACHECKER_H_ */

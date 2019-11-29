// Copyright (c) 2012-2019 LG Electronics, Inc.
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

#ifndef BUS_CLIENT_DB8_H_
#define BUS_CLIENT_DB8_H_

#include <luna-service2/lunaservice.hpp>
#include <boost/signals2.hpp>
#include <pbnjson.hpp>

#include "AbsLunaClient.h"
#include "interface/ISingleton.h"

using namespace LS;
using namespace pbnjson;

class DB8 : public ISingleton<DB8>,
            public AbsLunaClient {
friend class ISingleton<DB8>;
public:
    virtual ~DB8();

    bool insertLaunchPoint(pbnjson::JValue& json);
    bool updateLaunchPoint(const pbnjson::JValue& json);
    void deleteLaunchPoint(const string& launchPointId);

protected:
    // AbsLunaClient
    virtual void onInitialzed() override;
    virtual void onFinalized() override;
    virtual void onServerStatusChanged(bool isConnected) override;

private:
    static const char* KIND_NAME;

    static bool onResponse(LSHandle* sh, LSMessage* message, void* context);

    static bool onFind(LSHandle* sh, LSMessage* message, void* context);
    void find();

    static bool onPutKind(LSHandle* sh, LSMessage* message, void* context);
    void putKind();

    static bool onPutPermissions(LSHandle* sh, LSMessage* message, void* context);
    void putPermissions();

    DB8();

};

#endif /* BUS_CLIENT_DB8_H_ */

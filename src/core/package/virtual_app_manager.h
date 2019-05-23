// Copyright (c) 2017-2018 LG Electronics, Inc.
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

#ifndef CORE_PACKAGE_VIRTUAL_APP_MANAGER_H_
#define CORE_PACKAGE_VIRTUAL_APP_MANAGER_H_

#include <string>

#include <luna-service2/lunaservice.h>
#include <pbnjson.hpp>
#include <vector>

#include "core/base/singleton.h"

enum class VirtualAppType
    : int {
        REGULAR = 0, TEMP,
};

class VirtualAppRequest {
public:
    VirtualAppRequest(const VirtualAppType _type, const pbnjson::JValue& _jmsg, LSMessage* _lsmsg) :
            app_type(_type), jmsg(_jmsg), lsmsg(_lsmsg), async_job_token(0), error_code(0), error_text("")
    {
        if (lsmsg != NULL)
            LSMessageRef(lsmsg);
    }

    ~VirtualAppRequest()
    {
        if (lsmsg != NULL) {
            LSMessageUnref(lsmsg);
            lsmsg = NULL;
        }
    }

    VirtualAppType app_type;
    pbnjson::JValue jmsg;
    LSMessage* lsmsg;
    LSMessageToken async_job_token;
    int error_code;
    std::string error_text;
};
typedef std::shared_ptr<VirtualAppRequest> VirtualAppRequestPtr;

class VirtualAppManager: public Singleton<VirtualAppManager> {
public:
    VirtualAppManager();
    ~VirtualAppManager();

    void InstallTmpVirtualAppOnLaunch(const pbnjson::JValue& jmsg, LSMessage* lsmsg);
    void InstallVirtualApp(const pbnjson::JValue& jmsg, LSMessage* lsmsg);
    void UninstallVirtualApp(const pbnjson::JValue& jmsg, LSMessage* lsmsg);
    void RemoveVirtualAppPackage(const std::string& app_id, VirtualAppType app_type);

private:
    friend class Singleton<VirtualAppManager> ;

    static bool OnTempPackageReady(LSHandle* lshandle, LSMessage* lsmsg, void* user_data);
    static bool OnPackageReady(LSHandle* lshandle, LSMessage* lsmsg, void* user_data);
    static bool OnLSConfigRemoved(LSHandle* lshandle, LSMessage* lsmsg, void* user_data);
    VirtualAppRequestPtr MakeVirtualAppRequest(VirtualAppType app_type, const pbnjson::JValue& jmsg, LSMessage* lsmsg);

    bool ValidataHostApp(const std::string& host_id);
    VirtualAppRequestPtr GetWorkItemByToken(LSMessageToken token);
    std::string GetAppBasePath(VirtualAppType app_type);
    std::string GetDeviceId(VirtualAppType app_type);
    void FinishJob(VirtualAppRequestPtr va_request, bool need_reply = true);

    void ReplyForRequest(LSMessage* lsmsg, int error_code, const std::string& error_text);

    void CreateVirtualApp(VirtualAppRequestPtr request);
    bool CreateVirtualAppPackage(const std::string& app_id, const std::string& host_id, const pbnjson::JValue& app_info, const pbnjson::JValue& launch_params, VirtualAppType app_type,
            std::string& error_text);
    bool CreateLSConfig(const std::string& app_id, const std::string& device_id, LSFilterFunc cb_func, LSMessageToken& msg_token);

    bool RemoveLSConfig(const std::string& app_id, const std::string& device_id, LSFilterFunc cb_func, LSMessageToken* msg_token);

    std::vector<VirtualAppRequestPtr> work_items_;
};

#endif // CORE_PACKAGE_VIRTUAL_APP_MANAGER_H_

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

#ifndef APP_LAUNCHING_ITEM_H_
#define APP_LAUNCHING_ITEM_H_

#include <luna-service2/lunaservice.h>
#include <list>
#include <pbnjson.hpp>

#include "core/lifecycle/application_errors.h"
#include "core/package/application_description.h"

const std::string SYS_LAUNCHING_UID = "alertId";

enum class AppLaunchRequestType {
    INTERNAL = 0, EXTERNAL, EXTERNAL_FOR_VIRTUALAPP,
};

enum class AppLaunchingStage {
    NONE = 0, PRELAUNCH, MEMORY_CHECK, LAUNCH, DONE,
};

class AppLaunchingItem {
public:
    AppLaunchingItem(const std::string& app_id, AppLaunchRequestType rtype, const pbnjson::JValue& params, LSMessage* lsmsg);
    virtual ~AppLaunchingItem();

    const std::string& uid() const
    {
        return m_uid;
    }
    const std::string& app_id() const
    {
        return m_app_id;
    }
    const std::string& pid() const
    {
        return m_pid;
    }
    const std::string& requested_app_id() const
    {
        return m_requested_app_id;
    }
    bool is_redirected() const
    {
        return m_redirected;
    }
    const AppLaunchingStage& stage() const
    {
        return m_stage;
    }
    const int& sub_stage() const
    {
        return m_sub_stage;
    }
    const std::string& caller_id() const
    {
        return m_caller_id;
    }
    const std::string& caller_pid() const
    {
        return m_caller_pid;
    }
    bool show_splash() const
    {
        return m_show_splash;
    }
    bool show_spinner() const
    {
        return m_show_spinner;
    }
    const std::string& preload() const
    {
        return m_preload;
    }
    bool keep_alive() const
    {
        return m_keep_alive;
    }
    bool automatic_launch() const
    {
        return m_automatic_launch;
    }
    const pbnjson::JValue& params() const
    {
        return m_params;
    }
    LSMessage* lsmsg() const
    {
        return m_lsmsg;
    }
    LSMessageToken return_token() const
    {
        return m_return_token;
    }
    const pbnjson::JValue& return_jmsg() const
    {
        return m_return_jmsg;
    }
    int err_code() const
    {
        return m_err_code;
    }
    const std::string& err_text() const
    {
        return m_err_text;
    }
    const double& launch_start_time() const
    {
        return m_launch_start_time;
    }
    const std::string& launch_reason() const
    {
        return m_launch_reason;
    }
    bool is_last_input_app() const
    {
        return m_last_input_app;
    }

    // re-code redirection for app_desc and consider other values again
    bool set_redirection(const std::string& target_app_id, const pbnjson::JValue& new_params);
    void set_stage(AppLaunchingStage stage)
    {
        m_stage = stage;
    }
    void set_sub_stage(const int stage)
    {
        m_sub_stage = stage;
    }
    void set_pid(const std::string& pid)
    {
        m_pid = pid;
    }
    void set_caller_id(const std::string& id)
    {
        m_caller_id = id;
    }
    void set_caller_pid(const std::string& pid)
    {
        m_caller_pid = pid;
    }
    void set_show_splash(bool v)
    {
        m_show_splash = v;
    }
    void set_show_spinner(bool v)
    {
        m_show_spinner = v;
    }
    void set_preload(const std::string& preload)
    {
        m_preload = preload;
    }
    void set_keep_alive(bool v)
    {
        m_keep_alive = v;
    }
    void set_automatic_launch(bool v)
    {
        m_automatic_launch = v;
    }
    void set_return_token(LSMessageToken token)
    {
        m_return_token = token;
    }
    void reset_return_token()
    {
        m_return_token = 0;
    }
    void set_call_return_jmsg(const pbnjson::JValue& jmsg)
    {
        m_return_jmsg = jmsg.duplicate();
    }
    void set_err_code_text(int code, std::string err)
    {
        m_err_code = code;
        m_err_text = err;
    }
    void set_err_code(int code)
    {
        m_err_code = code;
    }
    void set_err_text(std::string err)
    {
        m_err_text = err;
    }
    void set_launch_start_time(const double& start_time)
    {
        m_launch_start_time = start_time;
    }
    void set_launch_reason(const std::string& launch_reason)
    {
        m_launch_reason = launch_reason;
    }
    void set_last_input_app(bool v)
    {
        m_last_input_app = v;
    }

private:
    std::string m_uid;
    std::string m_app_id;
    std::string m_pid;
    std::string m_requested_app_id;
    bool m_redirected;
    AppLaunchRequestType m_rtype;
    AppLaunchingStage m_stage;
    int m_sub_stage;
    pbnjson::JValue m_params;
    LSMessage* m_lsmsg;
    std::string m_caller_id;
    std::string m_caller_pid;
    bool m_show_splash;
    bool m_show_spinner;
    std::string m_preload;
    bool m_keep_alive;
    bool m_automatic_launch;
    LSMessageToken m_return_token;
    pbnjson::JValue m_return_jmsg;
    int m_err_code;
    std::string m_err_text;
    double m_launch_start_time;
    std::string m_launch_reason;
    bool m_last_input_app;
};

typedef std::shared_ptr<AppLaunchingItem> AppLaunchingItemPtr;
typedef std::list<AppLaunchingItemPtr> AppLaunchingItemList;

#endif

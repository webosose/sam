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

#ifndef APP_LAUNCHING_ITEM_H_
#define APP_LAUNCHING_ITEM_H_

#include <boost/function.hpp>
#include <lifecycle/ApplicationErrors.h>
#include <luna-service2/lunaservice.h>
#include <package/AppDescription.h>
#include <list>
#include <pbnjson.hpp>

const std::string SYS_LAUNCHING_UID = "alertId";

enum class AppLaunchingStage {
    INVALID = -1,
    CHECK_EXECUTE,
    PREPARE_PRELAUNCH = 0,
    PRELAUNCH,
    MEMORY_CHECK,
    MEMORY_CHECK_DONE,
    PRELAUNCH_DONE,
    LAUNCH,
    DONE,
};

enum class StageHandlerReturn : int {
    GO_NEXT_STAGE,
    GO_DEPENDENT_STAGE,
    REDIRECTED,
    ERROR,
};

enum class StageHandlerType : int {
    MAIN_CALL,
    SUB_CALL,
    SUB_BRIDGE_CALL,
    BRIDGE_CALL,
    DIRECT_CHECK,
};

class AppLaunchingItem;

typedef std::shared_ptr<AppLaunchingItem> AppLaunchingItemPtr;
typedef std::list<AppLaunchingItemPtr> AppLaunchingItemList;
typedef boost::function<bool(AppLaunchingItemPtr, pbnjson::JValue&)> PayloadMaker;
typedef boost::function<StageHandlerReturn(AppLaunchingItemPtr)> StageHandler;

struct StageItem {
    StageHandlerType m_handlerType;
    std::string m_uri;
    PayloadMaker m_payloadMaker;
    StageHandler m_handler;
    AppLaunchingStage m_launchingStage;

    StageItem(StageHandlerType _type, const std::string& uri, PayloadMaker maker, StageHandler handler, AppLaunchingStage stage)
        : m_handlerType(_type),
          m_uri(uri),
          m_payloadMaker(maker),
          m_handler(handler),
          m_launchingStage(stage)
    {
    }
};

typedef std::deque<StageItem> StageItemList;

class AppLaunchingItem {
public:
    AppLaunchingItem(const std::string& appId, const pbnjson::JValue& params, LSMessage* lsmsg);
    virtual ~AppLaunchingItem();

    const std::string& getUid() const
    {
        return m_uid;
    }
    const std::string& appId() const
    {
        return m_appId;
    }
    const std::string& getPid() const
    {
        return m_pid;
    }
    const std::string& requestedAppId() const
    {
        return m_requestedAppId;
    }
    bool isRedirected() const
    {
        return m_redirected;
    }
    const AppLaunchingStage& getStage() const
    {
        return m_stage;
    }
    const int& getSubStage() const
    {
        return m_subStage;
    }
    const std::string& getCallerId() const
    {
        return m_callerId;
    }
    const std::string& getCallerPid() const
    {
        return m_callerPid;
    }
    bool isShowSplash() const
    {
        return m_showSplash;
    }
    bool isShowSpinner() const
    {
        return m_showSpinner;
    }
    const std::string& getPreload() const
    {
        return m_preload;
    }
    bool isKeepAlive() const
    {
        return m_keepAlive;
    }
    bool isAutomaticLaunch() const
    {
        return m_automaticLaunch;
    }
    const pbnjson::JValue& getParams() const
    {
        return m_params;
    }
    LSMessage* lsmsg() const
    {
        return m_lsmsg;
    }
    LSMessageToken returnToken() const
    {
        return m_returnToken;
    }
    const pbnjson::JValue& returnJmsg() const
    {
        return m_returnJmsg;
    }
    int errCode() const
    {
        return m_errCode;
    }
    const std::string& errText() const
    {
        return m_errText;
    }
    const double& launchStartTime() const
    {
        return m_launchStartTime;
    }
    const std::string& launchReason() const
    {
        return m_launchReason;
    }
    bool isLastInputApp() const
    {
        return m_isLastInputApp;
    }

    // re-code redirection for app_desc and consider other values again
    bool setRedirection(const std::string& target_app_id, const pbnjson::JValue& new_params);
    void setStage(AppLaunchingStage stage)
    {
        m_stage = stage;
    }
    void setSubStage(const int stage)
    {
        m_subStage = stage;
    }
    void setPid(const std::string& pid)
    {
        m_pid = pid;
    }
    void setCallerId(const std::string& id)
    {
        m_callerId = id;
    }
    void setCallerPid(const std::string& pid)
    {
        m_callerPid = pid;
    }
    void setShowSplash(bool v)
    {
        m_showSplash = v;
    }
    void setShowSpinner(bool v)
    {
        m_showSpinner = v;
    }
    void setPreload(const std::string& preload)
    {
        m_preload = preload;
    }
    void setKeepAlive(bool v)
    {
        m_keepAlive = v;
    }
    void setAutomaticLaunch(bool v)
    {
        m_automaticLaunch = v;
    }
    void setReturnToken(LSMessageToken token)
    {
        m_returnToken = token;
    }
    void resetReturnToken()
    {
        m_returnToken = 0;
    }
    void setCallReturnJmsg(const pbnjson::JValue& jmsg)
    {
        m_returnJmsg = jmsg.duplicate();
    }
    void setErrCodeText(int code, std::string err)
    {
        m_errCode = code;
        m_errText = err;
    }
    void setErrCode(int code)
    {
        m_errCode = code;
    }
    void setErrText(std::string err)
    {
        m_errText = err;
    }
    void setLaunchStartTime(const double& start_time)
    {
        m_launchStartTime = start_time;
    }
    void setLaunchReason(const std::string& launch_reason)
    {
        m_launchReason = launch_reason;
    }
    void setLastInputApp(bool v)
    {
        m_isLastInputApp = v;
    }
    StageItemList& stageList()
    {
        return m_stageList;
    }
    void addStage(StageItem item)
    {
        m_stageList.push_back(item);
    }
    void clearAllStages()
    {
        m_stageList.clear();
    }

private:
    std::string m_uid;
    std::string m_appId;
    std::string m_pid;
    std::string m_requestedAppId;
    bool m_redirected;
    AppLaunchingStage m_stage;
    int m_subStage;
    pbnjson::JValue m_params;
    LSMessage* m_lsmsg;
    std::string m_callerId;
    std::string m_callerPid;
    bool m_showSplash;
    bool m_showSpinner;
    std::string m_preload;
    bool m_keepAlive;
    bool m_automaticLaunch;
    LSMessageToken m_returnToken;
    pbnjson::JValue m_returnJmsg;
    int m_errCode;
    std::string m_errText;
    double m_launchStartTime;
    std::string m_launchReason;
    bool m_isLastInputApp;

    StageItemList m_stageList;
};

#endif

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
#include <package/AppPackage.h>
#include <list>
#include <pbnjson.hpp>
#include "AppItem.h"

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

class LaunchAppItem;

typedef std::shared_ptr<LaunchAppItem> LaunchAppItemPtr;
typedef std::list<LaunchAppItemPtr> LaunchItemList;
typedef boost::function<bool(LaunchAppItemPtr, pbnjson::JValue&)> PayloadMaker;
typedef boost::function<StageHandlerReturn(LaunchAppItemPtr)> StageHandler;

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

class LaunchAppItem : public AppItem {
public:
    LaunchAppItem(const std::string& appId, const std::string& display, const pbnjson::JValue& params, LSMessage* message);
    virtual ~LaunchAppItem();

    const std::string& requestedAppId() const
    {
        return m_requestedAppId;
    }
    const AppLaunchingStage& getStage() const
    {
        return m_stage;
    }
    const int& getSubStage() const
    {
        return m_subStage;
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
    const pbnjson::JValue& getParams() const
    {
        return m_params;
    }
    LSMessage* getRequest() const
    {
        return m_request;
    }
    LSMessageToken returnToken() const
    {
        return m_returnToken;
    }
    int getErrorCode() const
    {
        return m_errorCode;
    }
    const std::string& getErrorText() const
    {
        return m_errorText;
    }
    const double& launchStartTime() const
    {
        return m_launchStartTime;
    }
    void setStage(AppLaunchingStage stage)
    {
        m_stage = stage;
    }
    void setSubStage(const int stage)
    {
        m_subStage = stage;
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
    void setReturnToken(LSMessageToken token)
    {
        m_returnToken = token;
    }
    void resetReturnToken()
    {
        m_returnToken = 0;
    }
    void setErrCodeText(int errorCode, std::string errorText)
    {
        m_errorCode = errorCode;
        m_errorText = errorText;
    }
    void setLaunchStartTime(const double& start_time)
    {
        m_launchStartTime = start_time;
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
    std::string m_requestedAppId;
    AppLaunchingStage m_stage;
    int m_subStage;
    pbnjson::JValue m_params;
    LSMessage* m_request;
    bool m_showSplash;
    bool m_showSpinner;
    std::string m_preload;
    bool m_keepAlive;
    LSMessageToken m_returnToken;
    int m_errorCode;
    std::string m_errorText;
    double m_launchStartTime;

    StageItemList m_stageList;
};

#endif

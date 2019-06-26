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

#include <bus/AppMgrService.h>
#include <bus/Notification.h>
#include <bus/ServiceObserver.h>
#include <lifecycle/AppInfoManager.h>
#include <lifecycle/stage/PrelauncherStages.h>
#include <package/AppDescription.h>
#include <package/PackageManager.h>
#include <setting/Settings.h>
#include <util/BaseLogs.h>
#include <util/JUtil.h>

bool PrelauncherStage::setPrelaunchingStages(LaunchAppItemPtr item)
{
    AppDescPtr appDesc = PackageManager::instance().getAppById(item->getAppId());
    if (!appDesc) {
        LOG_ERROR(MSGID_APPLAUNCH, 3,
                  PMLOGKS("app_id", item->getAppId().c_str()),
                  PMLOGKS("reason", "null_description"),
                  PMLOGKS("where", "set_prelaunching_stage"), "");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        return false;
    }

    // execution lock
    item->addStage(StageItem(StageHandlerType::DIRECT_CHECK,
                             "",
                             NULL,
                             handleExecutionLockStatus,
                             AppLaunchingStage::CHECK_EXECUTE)
    );
    return true;
}

StageHandlerReturn PrelauncherStage::handleExecutionLockStatus(LaunchAppItemPtr item)
{
    const std::string& app_id = item->getAppId();
    if (AppInfoManager::instance().canExecute(app_id) == false) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", app_id.c_str()),
                  PMLOGKS("reason", "execution_lock"),
                  PMLOGKS("where", "handle_execution_lock_status"), "");
        item->setErrCodeText(APP_ERR_LOCKED, "app is locked");
        return StageHandlerReturn::ERROR;
    }

    return StageHandlerReturn::GO_NEXT_STAGE;
}

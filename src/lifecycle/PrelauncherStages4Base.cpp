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
#include <BaseExtension.h>
#include <lifecycle/AppInfoManager.h>
#include <lifecycle/PrelauncherStages4Base.h>
#include <module/Notification.h>
#include <module/ServiceObserver.h>
#include <package/AppDescription.h>
#include <package/PackageManager.h>
#include <setting/Settings.h>
#include <util/BaseLogs.h>
#include <util/JUtil.h>

bool set_prelaunching_stages_4_base(AppLaunchingItem4BasePtr item)
{
    AppDescPtr appDesc = BaseExtension::instance().GetAppDesc(item->appId());
    if (!appDesc) {
        LOG_ERROR(MSGID_APPLAUNCH, 3,
                  PMLOGKS("app_id", item->appId().c_str()),
                  PMLOGKS("reason", "null_description"),
                  PMLOGKS("where", "set_prelaunching_stage"), "");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        return false;
    }

    // execution lock
    item->add_stage(StageItem4Base(StageHandlerType4Base::DIRECT_CHECK,
                                   "",
                                   NULL,
                                   handle_execution_lock_status,
                                   AppLaunchingStage4Base::CHECK_EXECUTE)
    );
    return true;
}

StageHandlerReturn4Base handle_execution_lock_status(AppLaunchingItem4BasePtr prelaunching_item)
{
    const std::string& app_id = prelaunching_item->appId();
    if (AppInfoManager::instance().canExecute(app_id) == false) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("reason", "execution_lock"), PMLOGKS("where", "handle_execution_lock_status"), "");
        prelaunching_item->setErrCodeText(APP_ERR_LOCKED, "app is locked");
        return StageHandlerReturn4Base::ERROR;
    }

    return StageHandlerReturn4Base::GO_NEXT_STAGE;
}

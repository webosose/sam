/* @@@LICENSE
 *
 * Copyright (c) 2024 LG Electronics, Inc.
 *
 * Confidential computer software. Valid license from LG required for
 * possession, use or copying. Consistent with FAR 12.211 and 12.212,
 * Commercial Computer Software, Computer Software Documentation, and
 * Technical Data for Commercial Items are licensed to the U.S. Government
 * under vendor's standard commercial license.
 *
 * LICENSE@@@
 */

#ifndef MANAGER_POLICYMANAGER_H_
#define MANAGER_POLICYMANAGER_H_

#include <iostream>
#include <utility>

#include "base/AppDescription.h"
#include "base/AppDescriptionList.h"
#include "base/LaunchPoint.h"
#include "base/LaunchPointList.h"
#include "base/LunaTask.h"
#include "base/LunaTaskList.h"
#include "base/RunningApp.h"
#include "base/RunningAppList.h"
#include "interface/ISingleton.h"
#include "interface/IClassName.h"

using namespace std;

class PolicyManager : public ISingleton<PolicyManager>,
                      public IClassName {
friend class ISingleton<PolicyManager> ;
public:
    virtual ~PolicyManager();

    void launch(LunaTaskPtr lunaTask);
    void pause(LunaTaskPtr lunaTask);
    void close(LunaTaskPtr lunaTask);
    void relaunch(LunaTaskPtr lunaTask);

    void removeLaunchPoint(LunaTaskPtr lunaTask);

private:
    PolicyManager();

    void onRequireMemory(LunaTaskPtr lunaTask);
    void onCloseForRemove(LunaTaskPtr lunaTask);

    void pre(LunaTaskPtr lunaTask);
    void onReplyWithIds(LunaTaskPtr lunaTask);
    void onReplyWithoutIds(LunaTaskPtr lunaTask);
};

#endif /* MANAGER_POLICYMANAGER_H_ */

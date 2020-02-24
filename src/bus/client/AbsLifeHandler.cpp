/* @@@LICENSE
 *
 * Copyright (c) 2020 LG Electronics, Inc.
 *
 * Confidential computer software. Valid license from LG required for
 * possession, use or copying. Consistent with FAR 12.211 and 12.212,
 * Commercial Computer Software, Computer Software Documentation, and
 * Technical Data for Commercial Items are licensed to the U.S. Government
 * under vendor's standard commercial license.
 *
 * LICENSE@@@
 */

#include "AbsLifeHandler.h"
#include "NativeContainer.h"
#include "WAM.h"

AbsLifeHandler& AbsLifeHandler::getLifeHandler(RunningAppPtr runningApp)
{
    AppType type = runningApp->getLaunchPoint()->getAppDesc()->getAppType();
    // handler_type
    switch (type) {
    case AppType::AppType_Web:
        return WAM::getInstance();

    case AppType::AppType_Native:
    case AppType::AppType_Native_AppShell:
    case AppType::AppType_Native_Builtin:
    case AppType::AppType_Native_Mvpd:
    case AppType::AppType_Native_Qml:
        return NativeContainer::getInstance();

    // TODO stub app handler should be defined.
    case AppType::AppType_Stub:
    default:
        return NativeContainer::getInstance();
    }
}

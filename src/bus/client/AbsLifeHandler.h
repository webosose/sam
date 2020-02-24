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

#ifndef BUS_CLIENT_ABSLIFEHANDLER_H_
#define BUS_CLIENT_ABSLIFEHANDLER_H_

#include <iostream>

#include "base/RunningApp.h"

using namespace std;

class AbsLifeHandler {
public:
    static AbsLifeHandler& getLifeHandler(RunningAppPtr runningApp);

    AbsLifeHandler() {};
    virtual ~AbsLifeHandler() {};

    virtual void launch(RunningAppPtr runningApp, LunaTaskPtr lunaTask) = 0;
    virtual void pause(RunningAppPtr runningApp, LunaTaskPtr lunaTask) = 0;
    virtual void close(RunningAppPtr runningApp, LunaTaskPtr lunaTask) = 0;
    virtual void kill(RunningAppPtr runningApp) = 0;

protected:

private:

};

#endif /* BUS_CLIENT_ABSLIFEHANDLER_H_ */

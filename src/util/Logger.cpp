// Copyright (c) 2019 LG Electronics, Inc.
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

#include "Logger.h"

#include <PmLogLib.h>

#include <string.h>


const char* Logger::LOG_KEY_SERVICE = "service";
const char* Logger::LOG_KEY_APPID = "appId";
const char* Logger::LOG_KEY_FUNC = "func";
const char* Logger::LOG_KEY_LINE = "line";
const char* Logger::LOG_KEY_ACTION = "action";
const char* Logger::LOG_KEY_REASON = "reason";
const char* Logger::LOG_KEY_TYPE = "type";
const char* Logger::LOG_KEY_ERRORCODE = "errorCode";
const char* Logger::LOG_KEY_ERRORTEXT = "errorText";
const char* Logger::LOG_KEY_PAYLOAD = "payload";

const char* Logger::NLID_LAUNCH_POINT_ADDED = "NL_LAUNCH_POINT_ADDED";
const char* Logger::NLID_APP_LAUNCH_BEGIN = "NL_APP_LAUNCH_BEGIN";
const char* Logger::MSGID_SAM_LOADING_SEQ = "SAM_LOADING_SEQ";
const char* Logger::MSGID_SERVICE_OBSERVER_INFORM = "SERVICE_OBSERVER_INFORM";
const char* Logger::MSGID_INTERNAL_ERROR = "INTERNAL_ERROR";
const char* Logger::MSGID_INTERNAL_WARNING = "INTERNAL_WARNING";
const char* Logger::MSGID_CONFIGD_INIT = "CONFIGD_INIT";
const char* Logger::MSGID_RECEIVED_UPDATE_INFO = "RECEIVED_UPDATE_INFO";
const char* Logger::MSGID_INVALID_UPDATE_STATUS_MSG = "INVALID_UPDATE_STATUS_MSG";
const char* Logger::MSGID_FAIL_WRITING_DONEDOWNLOAD = "FAIL_WRITING_DONEDOWNLOAD";
const char* Logger::MSGID_EULA_STATUS_CHECK = "EULA_STATUS_CHECK";
const char* Logger::MSGID_SETTING_INFO = "SETTING_INFO";
const char* Logger::MSGID_SETTINGS_ERR = "SETTINGS_ERR";
const char* Logger::MSGID_NO_APP_PATHS = "NO_APP_PATHS";
const char* Logger::MSGID_NO_BOOTTIMEAPPS = "NO_APPS_FOR_BOOTTIME";
const char* Logger::MSGID_BOOTSTATUS_RECEIVED = "BOOTSTATUS_RECEIVED";
const char* Logger::MSGID_RECEIVED_INVALID_SETTINGS = "RECEIVED_INVALID_SETTINGS";
const char* Logger::MSGID_FAIL_WRITING_DELETEDLIST = "FAIL_WRITING_DELETEDLIST";
const char* Logger::MSGID_RECEIVED_SYS_SIGNAL = "RECEIVED_SYS_SIGNAL";
const char* Logger::MSGID_REMOVE_FILE_ERR = "REMOVE_FILE_ERR";
const char* Logger::MSGID_LANGUAGE_SET_CHANGE = "LANGUAGE_SET_CHANGE";

/* service */
const char* Logger::MSGID_API_REQUEST = "API_REQUEST";
const char* Logger::MSGID_API_REQUEST_ERR = "API_REQUEST_ERR";
const char* Logger::MSGID_API_DEPRECATED = "API_DEPRECATED";
const char* Logger::MSGID_API_RETURN_FALSE = "API_RETURN_FALSE";

/* app life */
const char* Logger::MSGID_APPLAUNCH = "APP_LAUNCH";
const char* Logger::MSGID_APPLAUNCH_WARNING = "APP_LAUNCH_WARNING";
const char* Logger::MSGID_APPLAUNCH_ERR = "APP_LAUNCH_ERR";
const char* Logger::MSGID_APP_LAUNCHED = "APP_LAUNCHED";
const char* Logger::MSGID_APPCLOSE = "APP_CLOSE";
const char* Logger::MSGID_APPCLOSE_ERR = "APP_CLOSE_ERR";
const char* Logger::MSGID_APPPAUSE = "APP_PAUSE";
const char* Logger::MSGID_APPPAUSE_ERR = "APP_PAUSE_ERR";
const char* Logger::MSGID_APP_CLOSED = "APP_CLOSED";
const char* Logger::MSGID_WAM_RUNNING = "WAM_RUNNING";
const char* Logger::MSGID_WAM_RUNNING_ERR = "WAM_RUNNING_ERR";
const char* Logger::MSGID_RUNTIME_STATUS = "RUNTIME_STATUS";
const char* Logger::MSGID_LIFE_STATUS = "LIFE_STATUS";
const char* Logger::MSGID_LIFE_STATUS_ERR = "LIFE_STATUS_ERR";
const char* Logger::MSGID_APP_LIFESTATUS_REPLY_ERROR = "APP_LIFESTATUS_REPLY_ERROR";
const char* Logger::MSGID_APPINFO = "APPINFO";
const char* Logger::MSGID_APPINFO_ERR = "APPINFO_ERR";
const char* Logger::MSGID_CHANGE_APPID = "CHANGE_APPID";
const char* Logger::MSGID_CHANGE_APPID_ERR = "CHANGE_APPID_ERR";
const char* Logger::MSGID_LAUNCH_LASTAPP = "LAUNCH_LASTAPP";
const char* Logger::MSGID_LAUNCH_LASTAPP_ERR = "LAUNCH_LASTAPP_ERR";
const char* Logger::MSGID_RUNNING_LIST = "RUNNING_LIST";
const char* Logger::MSGID_RUNNING_LIST_ERR = "RUNNING_LIST_ERR";
const char* Logger::MSGID_FOREGROUND_INFO = "FOREGROUND_INFO";
const char* Logger::MSGID_LOADING_LIST = "LOADING_LIST";
const char* Logger::MSGID_GET_FOREGROUND_APPINFO = "GET_FOREGROUND_APPINFO";
const char* Logger::MSGID_GET_FOREGROUND_APP_ERR = "GET_FOREGROUND_APP_ERR" ;
const char* Logger::MSGID_SPLASH_TIMEOUT = "SPLASH_TIMEOUT";
const char* Logger::MSGID_APPLAUNCH_LOCK = "APPLAUNCH_LOCK";
const char* Logger::MSGID_NATIVE_APP_HANDLER = "NATIVE_APP_HANDLER";
const char* Logger::MSGID_NATIVE_APP_LIFE_CYCLE_EVENT = "NATIVE_APP_LIFE_CYCLE_EVENT";
const char* Logger::MSGID_NATIVE_CLIENT_INFO = "NATIVE_CLIENT_INFO";
const char* Logger::MSGID_HANDLE_CRIU = "HANDLE_CRIU";

/* app package */
const char* Logger::MSGID_START_SCAN = "START_SCAN";
const char* Logger::MSGID_APP_SCANNER = "APP_SCANNER";
const char* Logger::MSGID_APPSCAN_FAIL = "APP_SCAN_FAIL";
const char* Logger::MSGID_PACKAGE_LOAD = "PACKAGE_LOAD";
const char* Logger::MSGID_UNINSTALL_APP = "UNINSTALL_APP";
const char* Logger::MSGID_UNINSTALL_APP_ERR = "UNINSTALL_APP_ERR";
const char* Logger::MSGID_SECURITY_VIOLATION = "SEC_VIOLATION";
const char* Logger::MSGID_FAIL_GET_SELECTED_PROPS = "FAIL_GET_SELECTED_PROPS";
const char* Logger::MSGID_RESERVED_FOR_PRIVILEGED = "RESERVED_FOR_PRIVILEGED";
const char* Logger::MSGID_PACKAGE_STATUS = "PACKAGE_STATUS";

/* asm */
const char* Logger::MSGID_HANDLE_ASM = "HANDLE_ASM";
const char* Logger::MSGID_GENERATE_PASSPHRASE_ERR = "GENERATE_PASSPHRASE_ERR";
const char* Logger::MSGID_ECRYPTFS_ERR = "ECRYPTFS_ERR";
const char* Logger::MSGID_SHARED_ECRYPTFS_ERR = "SHARED_ECRYPTFS_ERR";

/* virtual app */
const char* Logger::MSGID_VIRTUAL_APP_WARNING = "VIRTUAL_APP_WARNING";
const char* Logger::MSGID_TMP_VIRTUAL_APP_LAUNCH = "TMP_VIRTUAL_APP_LAUNCH";
const char* Logger::MSGID_ADD_VIRTUAL_APP = "ADD_VIRTUAL_APP";
const char* Logger::MSGID_REMOVE_VIRTUAL_APP = "REMOVE_VIRTUAL_APP";
const char* Logger::MSGID_VIRTUAL_APP_INFO_CREATED = "VIRTUAL_APP_INFO_CREATED";
const char* Logger::MSGID_VIRTUAL_APP_RETURN = "VIRTUAL_APP_RETURN";

/* base db */
const char* Logger::MSGID_DB_LOAD = "DB_LOAD";
const char* Logger::MSGID_DB_LOAD_ERR = "DB_LOAD_ERR";

/* launch point manager */
const char* Logger::MSGID_LAUNCH_POINT_REQUEST = "LAUNCH_POINT_REQUEST";
const char* Logger::MSGID_LAUNCH_POINT = "LAUNCH_POINT";
const char* Logger::MSGID_LAUNCH_POINT_WARNING = "LAUNCH_POINT_WARNING";
const char* Logger::MSGID_LAUNCH_POINT_ERROR = "LAUNCH_POINT_ERROR";
const char* Logger::MSGID_LAUNCH_POINT_ACTION = "LAUNCH_POINT_ACTION";
const char* Logger::MSGID_LAUNCH_POINT_ADDED = "LAUNCH_POINT_ADDED";
const char* Logger::MSGID_LAUNCH_POINT_UPDATED = "LAUNCH_POINT_UPDATED";
const char* Logger::MSGID_LAUNCH_POINT_REMOVED = "LAUNCH_POINT_REMOVED";
const char* Logger::MSGID_LAUNCH_POINT_MOVED = "LAUNCH_POINT_MOVED";
const char* Logger::MSGID_LAUNCH_POINT_DB_HANDLE = "LAUNCH_POINT_DB_HANDLE";
const char* Logger::MSGID_LAUNCH_POINT_REPLY_SUBSCRIBER = "LAUNCH_POINT_REPLY_SUBSCRIBER";

/* ls-service */
const char* Logger::MSGID_LUNA_SUBSCRIPTION = "LUNA_SUBSCRIPTION";
const char* Logger::MSGID_SUBSCRIPTION_REPLY = "SUBSCRIPTION_REPLY";
const char* Logger::MSGID_SUBSCRIPTION_REPLY_ERR = "SUBSCRIPTION_REPLY_ERR";
const char* Logger::MSGID_LSCALL_ERR = "LSCALL_ERR";
const char* Logger::MSGID_LSCALL_RETURN_FAIL = "LSCALL_RETURN_FAIL";
const char* Logger::MSGID_GENERAL_JSON_PARSING_ERROR = "GENERAL_JSON_PARSING_ERROR";
const char* Logger::MSGID_DEPRECATED_API = "DEPRECATED_API";
const char* Logger::MSGID_SRVC_REGISTER_FAIL = "SRVC_REGISTER_FAIL";
const char* Logger::MSGID_SRVC_CATEGORY_FAIL = "SRVC_CATEGORY_FAIL";
const char* Logger::MSGID_SRVC_ATTACH_FAIL = "SRVC_ATTACH_FAIL";
const char* Logger::MSGID_SRVC_DETACH_FAIL = "SRVC_DETACH_FAIL";
const char* Logger::MSGID_OBJECT_CASTING_FAIL = "OBJECT_CASTING_FAIL";

/* notification */
const char* Logger::MSGID_NOTIFICATION = "NOTIFICATION";
const char* Logger::MSGID_NOTIFICATION_FAIL = "NOTIFICATION_FAIL";

/* call chain */
const char* Logger::MSGID_CALLCHAIN_ERR = "CALLCHAIN_ERR";

const char* Logger::MSGID_TV_CONFIGURATION = "TV_CONFIGURATION";
const char* Logger::MSGID_GET_LASTINPUT_FAIL = "GET_LASTINPUT_FAIL";
const char* Logger::MSGID_LAUNCH_LASTINPUT_ERR = "LAUNCH_LASTINPUT_ERR";
const char* Logger::MSGID_APPSCAN_FILTER_TV = "APPSCAN_FILTER_TV";
const char* Logger::MSGID_CLEAR_FIRST_LAUNCHING_APP = "CLEAR_FIRST_LAUNCHNIG_APP";

/* package */
const char* Logger::MSGID_PER_APP_SETTINGS = "PER_APP_SETTINGS";

/* launch point */
const char* Logger::MSGID_LAUNCH_POINT_ORDERING = "LAUNCH_POINT_ORDERING";
const char* Logger::MSGID_LAUNCH_POINT_ORDERING_WARNING = "LAUNCH_POINT_ORDERING_WARNING";
const char* Logger::MSGID_LAUNCH_POINT_ORDERING_ERROR = "LAUNCH_POINT_ORDERING_ERROR";

/* customization */
const char* Logger::MSGID_LOAD_ICONS_FAIL = "LOAD_ICONS_FAIL";

const string Logger::EMPTY = "";

void Logger::debug(const string& className, const string& functionName, const string& what)
{
    getInstance().write(LogLevel_DEBUG, className, functionName, EMPTY, what, EMPTY);
}

void Logger::info(const string& className, const string& functionName, const string& what)
{
    getInstance().write(LogLevel_INFO, className, functionName, EMPTY, what, EMPTY);
}

void Logger::warning(const string& className, const string& functionName, const string& what)
{
    getInstance().write(LogLevel_WARNING, className, functionName, EMPTY, what, EMPTY);
}

void Logger::error(const string& className, const string& functionName, const string& what)
{
    getInstance().write(LogLevel_ERROR, className, functionName, EMPTY, what, EMPTY);
}

void Logger::debug(const string& className, const string& functionName, const string& who, const string& what, const string& detail)
{
    getInstance().write(LogLevel_DEBUG, className, functionName, who, what, detail);
}

void Logger::info(const string& className, const string& functionName, const string& who, const string& what, const string& detail)
{
    getInstance().write(LogLevel_INFO, className, functionName, who, what, detail);
}

void Logger::warning(const string& className, const string& functionName, const string& who, const string& what, const string& detail)
{
    getInstance().write(LogLevel_WARNING, className, functionName, who, what, detail);
}

void Logger::error(const string& className, const string& functionName, const string& who, const string& what, const string& detail)
{
    getInstance().write(LogLevel_ERROR, className, functionName, who, what, detail);
}

const string& Logger::toString(const enum LogLevel& level)
{
    static const string DEBUG = "[D]";
    static const string INFO = "[I]";
    static const string WARNING = "[W]";
    static const string ERROR = "[E]";

    switch(level) {
    case LogLevel_DEBUG:
        return DEBUG;

    case LogLevel_INFO:
        return INFO;

    case LogLevel_WARNING:
        return WARNING;

    case LogLevel_ERROR:
       return ERROR;
    }
    return DEBUG;
}

Logger::Logger()
    : m_level(LogLevel_DEBUG),
      m_type(LogType_CONSOLE)
{
}

Logger::~Logger()
{
}

void Logger::setLevel(enum LogLevel level)
{
    m_level = level;
}

void Logger::setType(enum LogType type)
{
    m_type = type;
}

void Logger::write(const enum LogLevel& level, const string& className, const string& functionName, const string& who, const string& what, const string& detail)
{
    if (level < m_level)
        return;

    switch (m_type) {
    case LogType_CONSOLE:
        writeConsole(level, className, functionName, who, what, detail);
        break;

    case LogType_PMLOG:
        writePmlog(level, className, functionName, who, what, detail);
        break;

    default:
        cerr << "Unsupported Log Type" << endl;
        break;
    }
}

void Logger::writeConsole(const enum LogLevel& level, const string& className, const string& functionName, const string& who, const string& what, const string& detail)
{
    const string& str = toString(level);
    if (who.empty() && detail.empty()) {
        printf("[%s][%s][%s] %s\n", str.c_str(), className.c_str(), functionName.c_str(), what.c_str());
    } else if (who.empty()) {
        printf("[%s][%s][%s] %s\n%s\n", str.c_str(), className.c_str(), functionName.c_str(), what.c_str(), detail.c_str());
    } else if (detail.empty()) {
        printf("[%s][%s][%s][%s] %s\n", str.c_str(), className.c_str(), functionName.c_str(), who.c_str(), what.c_str());
    } else {
        printf("[%s][%s][%s][%s] %s\n%s\n", str.c_str(), className.c_str(), functionName.c_str(), who.c_str(), what.c_str(), detail.c_str());
    }
}

void Logger::writePmlog(const enum LogLevel& level, const string& className, const string& functionName, const string& who, const string& what, const string& detail)
{
    static PmLogContext context = nullptr;
    if (context == nullptr) {
        context = PmLogGetContextInline("SAM");
    }

    // TODO handle empty who
    switch(level) {
    case LogLevel_DEBUG:
        PmLogDebug(context, detail.c_str());
        break;

    case LogLevel_INFO:
        if (detail.empty()) {
            PmLogInfo(context, className.c_str(), 3,
                      PMLOGKS("function", functionName.c_str()),
                      PMLOGKS("who", who.c_str()),
                      PMLOGKS("what", what.c_str()), "");
        } else {
            PmLogInfo(context, className.c_str(), 3,
                      PMLOGKS("function", functionName.c_str()),
                      PMLOGKS("who", who.c_str()),
                      PMLOGKS("what", what.c_str()), "%s", detail.c_str());
        }
        break;

    case LogLevel_WARNING:
        if (detail.empty()) {
            PmLogWarning(context, className.c_str(), 3,
                         PMLOGKS("function", functionName.c_str()),
                         PMLOGKS("who", who.c_str()),
                         PMLOGKS("what", what.c_str()), "");
        } else {
            PmLogWarning(context, className.c_str(), 3,
                         PMLOGKS("function", functionName.c_str()),
                         PMLOGKS("who", who.c_str()),
                         PMLOGKS("what", what.c_str()), "%s", detail.c_str());
        }
        break;

    case LogLevel_ERROR:
        if (detail.empty()) {
            PmLogError(context, className.c_str(), 3,
                       PMLOGKS("function", functionName.c_str()),
                       PMLOGKS("who", who.c_str()),
                       PMLOGKS("what", what.c_str()), "");
        } else {
            PmLogError(context, className.c_str(), 3,
                       PMLOGKS("function", functionName.c_str()),
                       PMLOGKS("who", who.c_str()),
                       PMLOGKS("what", what.c_str()), "%s", detail.c_str());
        }
        break;
    }
}

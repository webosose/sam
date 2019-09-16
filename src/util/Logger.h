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

#ifndef UTIL_LOGGER_H_
#define UTIL_LOGGER_H_

#include <iostream>
#include <map>

using namespace std;

enum LogLevel {
    LogLevel_DEBUG,
    LogLevel_INFO,
    LogLevel_WARNING,
    LogLevel_ERROR,
};

enum LogType {
    LogType_CONSOLE,
    LogType_PMLOG
};

class Logger {
public:
    static const char* LOG_KEY_SERVICE;
    static const char* LOG_KEY_APPID;
    static const char* LOG_KEY_FUNC;
    static const char* LOG_KEY_LINE;
    static const char* LOG_KEY_ACTION;
    static const char* LOG_KEY_REASON;
    static const char* LOG_KEY_TYPE;
    static const char* LOG_KEY_ERRORCODE;
    static const char* LOG_KEY_ERRORTEXT;
    static const char* LOG_KEY_PAYLOAD;

    static const char* NLID_LAUNCH_POINT_ADDED;
    static const char* NLID_APP_LAUNCH_BEGIN;
    static const char* MSGID_SAM_LOADING_SEQ;
    static const char* MSGID_SERVICE_OBSERVER_INFORM;
    static const char* MSGID_INTERNAL_ERROR;
    static const char* MSGID_INTERNAL_WARNING;
    static const char* MSGID_CONFIGD_INIT;
    static const char* MSGID_RECEIVED_UPDATE_INFO;
    static const char* MSGID_INVALID_UPDATE_STATUS_MSG;
    static const char* MSGID_FAIL_WRITING_DONEDOWNLOAD;
    static const char* MSGID_EULA_STATUS_CHECK;
    static const char* MSGID_SETTING_INFO;
    static const char* MSGID_SETTINGS_ERR;
    static const char* MSGID_NO_APP_PATHS;
    static const char* MSGID_NO_BOOTTIMEAPPS;
    static const char* MSGID_BOOTSTATUS_RECEIVED;
    static const char* MSGID_RECEIVED_INVALID_SETTINGS;
    static const char* MSGID_FAIL_WRITING_DELETEDLIST;
    static const char* MSGID_RECEIVED_SYS_SIGNAL;
    static const char* MSGID_REMOVE_FILE_ERR;
    static const char* MSGID_LANGUAGE_SET_CHANGE;

    /* service */
    static const char* MSGID_API_REQUEST;
    static const char* MSGID_API_REQUEST_ERR;
    static const char* MSGID_API_DEPRECATED;
    static const char* MSGID_API_RETURN_FALSE;

    /* app life */
    static const char* MSGID_APPLAUNCH;
    static const char* MSGID_APPLAUNCH_WARNING;
    static const char* MSGID_APPLAUNCH_ERR;
    static const char* MSGID_APP_LAUNCHED;
    static const char* MSGID_APPCLOSE;
    static const char* MSGID_APPCLOSE_ERR;
    static const char* MSGID_APPPAUSE;
    static const char* MSGID_APPPAUSE_ERR;
    static const char* MSGID_APP_CLOSED;
    static const char* MSGID_WAM_RUNNING;
    static const char* MSGID_WAM_RUNNING_ERR;
    static const char* MSGID_RUNTIME_STATUS;
    static const char* MSGID_LIFE_STATUS;
    static const char* MSGID_LIFE_STATUS_ERR;
    static const char* MSGID_APP_LIFESTATUS_REPLY_ERROR;
    static const char* MSGID_APPINFO;
    static const char* MSGID_APPINFO_ERR;
    static const char* MSGID_CHANGE_APPID;
    static const char* MSGID_CHANGE_APPID_ERR;
    static const char* MSGID_LAUNCH_LASTAPP;
    static const char* MSGID_LAUNCH_LASTAPP_ERR;
    static const char* MSGID_RUNNING_LIST;
    static const char* MSGID_RUNNING_LIST_ERR;
    static const char* MSGID_FOREGROUND_INFO;
    static const char* MSGID_LOADING_LIST;
    static const char* MSGID_GET_FOREGROUND_APPINFO;
    static const char* MSGID_GET_FOREGROUND_APP_ERR;
    static const char* MSGID_SPLASH_TIMEOUT;
    static const char* MSGID_APPLAUNCH_LOCK;
    static const char* MSGID_NATIVE_APP_HANDLER;
    static const char* MSGID_NATIVE_APP_LIFE_CYCLE_EVENT;
    static const char* MSGID_NATIVE_CLIENT_INFO;
    static const char* MSGID_HANDLE_CRIU;

    /* app package */
    static const char* MSGID_START_SCAN;
    static const char* MSGID_APP_SCANNER;
    static const char* MSGID_APPSCAN_FAIL;
    static const char* MSGID_PACKAGE_LOAD;
    static const char* MSGID_UNINSTALL_APP;
    static const char* MSGID_UNINSTALL_APP_ERR;
    static const char* MSGID_SECURITY_VIOLATION;
    static const char* MSGID_FAIL_GET_SELECTED_PROPS;
    static const char* MSGID_RESERVED_FOR_PRIVILEGED;
    static const char* MSGID_PACKAGE_STATUS;

    /* asm */
    static const char* MSGID_HANDLE_ASM;
    static const char* MSGID_GENERATE_PASSPHRASE_ERR;
    static const char* MSGID_ECRYPTFS_ERR;
    static const char* MSGID_SHARED_ECRYPTFS_ERR;

    /* virtual app */
    static const char* MSGID_VIRTUAL_APP_WARNING;
    static const char* MSGID_TMP_VIRTUAL_APP_LAUNCH;
    static const char* MSGID_ADD_VIRTUAL_APP;
    static const char* MSGID_REMOVE_VIRTUAL_APP;
    static const char* MSGID_VIRTUAL_APP_INFO_CREATED;
    static const char* MSGID_VIRTUAL_APP_RETURN;

    /* base db */
    static const char* MSGID_DB_LOAD;
    static const char* MSGID_DB_LOAD_ERR;

    /* launch point manager */
    static const char* MSGID_LAUNCH_POINT_REQUEST;
    static const char* MSGID_LAUNCH_POINT;
    static const char* MSGID_LAUNCH_POINT_WARNING;
    static const char* MSGID_LAUNCH_POINT_ERROR;
    static const char* MSGID_LAUNCH_POINT_ACTION;
    static const char* MSGID_LAUNCH_POINT_ADDED;
    static const char* MSGID_LAUNCH_POINT_UPDATED;
    static const char* MSGID_LAUNCH_POINT_REMOVED;
    static const char* MSGID_LAUNCH_POINT_MOVED;
    static const char* MSGID_LAUNCH_POINT_DB_HANDLE;
    static const char* MSGID_LAUNCH_POINT_REPLY_SUBSCRIBER;

    /* ls-service */
    static const char* MSGID_LUNA_SUBSCRIPTION;
    static const char* MSGID_SUBSCRIPTION_REPLY;
    static const char* MSGID_SUBSCRIPTION_REPLY_ERR;;
    static const char* MSGID_LSCALL_ERR;
    static const char* MSGID_LSCALL_RETURN_FAIL;
    static const char* MSGID_GENERAL_JSON_PARSING_ERROR;
    static const char* MSGID_DEPRECATED_API;
    static const char* MSGID_SRVC_REGISTER_FAIL;
    static const char* MSGID_SRVC_CATEGORY_FAIL;
    static const char* MSGID_SRVC_ATTACH_FAIL;
    static const char* MSGID_SRVC_DETACH_FAIL;
    static const char* MSGID_OBJECT_CASTING_FAIL;

    /* notification */
    static const char* MSGID_NOTIFICATION;
    static const char* MSGID_NOTIFICATION_FAIL;

    /* call chain */
    static const char* MSGID_CALLCHAIN_ERR;

    static const char* MSGID_TV_CONFIGURATION;
    static const char* MSGID_GET_LASTINPUT_FAIL;
    static const char* MSGID_LAUNCH_LASTINPUT_ERR;
    static const char* MSGID_APPSCAN_FILTER_TV;
    static const char* MSGID_CLEAR_FIRST_LAUNCHING_APP;

    /* package */
    static const char* MSGID_PER_APP_SETTINGS;

    /* launch point */
    static const char* MSGID_LAUNCH_POINT_ORDERING;
    static const char* MSGID_LAUNCH_POINT_ORDERING_WARNING;
    static const char* MSGID_LAUNCH_POINT_ORDERING_ERROR;

    /* customization */
    static const char* MSGID_LOAD_ICONS_FAIL;


    static const int API_ERR_CODE_GENERAL = 1;
    static const int API_ERR_CODE_INVALID_PAYLOAD= 2;
    static const int API_ERR_CODE_DEPRECATED = 999;

    template<typename ... Args>
    static const string format(const std::string& format, Args ... args)
    {
        static char buffer[1024];
        snprintf(buffer, 1024, format.c_str(), args ... );
        return std::string(buffer);
    }

    static void debug(const string& className, const string& functionName, const string& what);
    static void info(const string& className, const string& functionName, const string& what);
    static void warning(const string& className, const string& functionName, const string& what);
    static void error(const string& className, const string& functionName, const string& what);

    static void debug(const string& className, const string& functionName, const string& who, const string& what, const string& detail = EMPTY);
    static void info(const string& className, const string& functionName, const string& who, const string& what, const string& detail = EMPTY);
    static void warning(const string& className, const string& functionName, const string& who, const string& what, const string& detail = EMPTY);
    static void error(const string& className, const string& functionName, const string& who, const string& what, const string& detail = EMPTY);

    virtual ~Logger();

    static Logger& getInstance()
    {
        static Logger _instance;
        return _instance;
    }

    void setLevel(enum LogLevel level);
    void setType(enum LogType type);

private:
    static const string EMPTY;

    static const string& toString(const enum LogLevel& level);

    Logger();

    void write(const enum LogLevel& level, const string& className, const string& functionName, const string& who, const string& what, const string& detail);
    void writeConsole(const enum LogLevel& level, const string& className, const string& functionName, const string& who, const string& what, const string& detail);
    void writePmlog(const enum LogLevel& level, const string& className, const string& functionName, const string& who, const string& what, const string& detail);

    enum LogLevel m_level;
    enum LogType m_type;
};

#endif /* UTIL_LOGGER_H_ */

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

#ifndef LOGGING_H
#define LOGGING_H

#include <PmLogLib.h>

#define LOG_KEY_SERVICE     "service"
#define LOG_KEY_APPID       "appId"
#define LOG_KEY_FUNC        "func"
#define LOG_KEY_LINE        "line"
#define LOG_KEY_ACTION      "action"
#define LOG_KEY_REASON      "reason"
#define LOG_KEY_TYPE        "type"
#define LOG_KEY_ERRORCODE   "errorCode"
#define LOG_KEY_ERRORTEXT   "errorText"
#define LOG_KEY_PAYLOAD     "payload"

#define LOG_NORMAL(...)              PmLogInfo(GetSAMPmLogContext(), ##__VA_ARGS__)
#define LOG_INFO(...)                PmLogInfo(GetSAMPmLogContext(), ##__VA_ARGS__)
#define LOG_INFO_WITH_CLOCK(...)     PmLogInfoWithClock(GetSAMPmLogContext(), ##__VA_ARGS__)
#define LOG_WARNING(...)             PmLogWarning(GetSAMPmLogContext(), ##__VA_ARGS__)
#define LOG_ERROR(...)               PmLogError(GetSAMPmLogContext(), ##__VA_ARGS__)
#define LOG_CRITICAL(...)            PmLogCritical(GetSAMPmLogContext(), ##__VA_ARGS__)
#define LOG_DEBUG(...)               PmLogDebug(GetSAMPmLogContext(), ##__VA_ARGS__)

/// list of msg ids

/* normal messages */
///////////////////////////////////////////////////////////////////
// Note: do not add/modify/remove without compliance discussion
#define NLID_LAUNCH_POINT_ADDED             "NL_LAUNCH_POINT_ADDED" /** request for adding launch point */
#define NLID_APP_LAUNCH_BEGIN               "NL_APP_LAUNCH_BEGIN" /** Beginning of application launch process */
///////////////////////////////////////////////////////////////////

/* common & setting */
#define MSGID_SAM_LOADING_SEQ               "SAM_LOADING_SEQ" /** sam loading sequence */
#define MSGID_SERVICE_OBSERVER_INFORM       "SERVICE_OBSERVER_INFORM" /** inform service connection status */
#define MSGID_INTERNAL_ERROR                "INTERNAL_ERROR" /** unpredictable case error */
#define MSGID_INTERNAL_WARNING              "INTERNAL_WARNING" /** unpredictable case warning (less critical than error) */
#define MSGID_CONFIGD_INIT                  "CONFIGD_INIT" /** load setting values from configd */
#define MSGID_RECEIVED_UPDATE_INFO          "RECEIVED_UPDATE_INFO" /** received update info from updaterServer */
#define MSGID_INVALID_UPDATE_STATUS_MSG     "INVALID_UPDATE_STATUS_MSG" /** Received invalid update status message */
#define MSGID_FAIL_WRITING_DONEDOWNLOAD     "FAIL_WRITING_DONEDOWNLOAD" /** error when writing done of download */
#define MSGID_EULA_STATUS_CHECK             "EULA_STATUS_CHECK" /** eula status value */
#define MSGID_SETTING_INFO                  "SETTING_INFO" /** setting info */
#define MSGID_SETTINGS_ERR                  "SETTINGS_ERR" /** Failure to read settings file */
#define MSGID_NO_APP_PATHS                  "NO_APP_PATHS" /** No ApplicationPaths defined in settings file */
#define MSGID_NO_BOOTTIMEAPPS               "NO_APPS_FOR_BOOTTIME" /** There are no applications in Boot Time App list */
#define MSGID_BOOTSTATUS_RECEIVED           "BOOTSTATUS_RECEIVED" /** info about bootstatus from bootd */
#define MSGID_RECEIVED_INVALID_SETTINGS     "RECEIVED_INVALID_SETTINGS" /** receive invalid value from settings service */
#define MSGID_FAIL_WRITING_DELETEDLIST      "FAIL_WRITING_DELETEDLIST" /** error when writing deleted list of system apps */
#define MSGID_RECEIVED_SYS_SIGNAL           "RECEIVED_SYS_SIGNAL" /* received system signal */
#define MSGID_REMOVE_FILE_ERR               "REMOVE_FILE_ERR" /* Failure to remove file */
#define MSGID_LANGUAGE_SET_CHANGE           "LANGUAGE_SET_CHANGE" /* set language info  */

/* service */
#define MSGID_API_REQUEST                   "API_REQUEST" /* service api request */
#define MSGID_API_REQUEST_ERR               "API_REQUEST_ERR" /* service api request error */
#define MSGID_API_DEPRECATED                "API_DEPRECATED"
#define MSGID_API_RETURN_FALSE              "API_RETURN_FALSE"

/* app life */
#define MSGID_APPLAUNCH                     "APP_LAUNCH" /** app launch sequence */
#define MSGID_APPLAUNCH_WARNING             "APP_LAUNCH_WARNING" /** app launch sequence warning */
#define MSGID_APPLAUNCH_ERR                 "APP_LAUNCH_ERR" /** app launch sequence error */
#define MSGID_APP_LAUNCHED                  "APP_LAUNCHED" /** app launch done */
#define MSGID_APPCLOSE                      "APP_CLOSE" /** app close sequence */
#define MSGID_APPCLOSE_ERR                  "APP_CLOSE_ERR" /* app close sequence error */
#define MSGID_APPPAUSE                      "APP_PAUSE"
#define MSGID_APPPAUSE_ERR                  "APP_PAUSE_ERR"
#define MSGID_APP_CLOSED                    "APP_CLOSED" /* app close sequence error */
#define MSGID_WAM_RUNNING                   "WAM_RUNNING" /** handing webapp running list sent by wam */
#define MSGID_WAM_RUNNING_ERR               "WAM_RUNNING_ERR" /** error handing web app running list from sent by wam  */
#define MSGID_RUNTIME_STATUS                "RUNTIME_STATUS" /** app runtime status change */
#define MSGID_LIFE_STATUS                   "LIFE_STATUS" /** app life status change */
#define MSGID_LIFE_STATUS_ERR               "LIFE_STATUS_ERR" /** app life status change error */
#define MSGID_APP_LIFESTATUS_REPLY_ERROR    "APP_LIFESTATUS_REPLY_ERROR" /** error about fail to reply appLifeStatus */
#define MSGID_APPINFO                       "APPINFO" /** app info change */
#define MSGID_APPINFO_ERR                   "APPINFO_ERR" /** app info chnage error */
#define MSGID_CHANGE_APPID                  "CHANGE_APPID" /** change running app id */
#define MSGID_CHANGE_APPID_ERR              "CHANGE_APPID_ERR" /** change running app id error */
#define MSGID_LAUNCH_LASTAPP                "LAUNCH_LASTAPP" /** last app launching starts */
#define MSGID_LAUNCH_LASTAPP_ERR            "LAUNCH_LASTAPP_ERR" /** error about launching last app (used by MM) */
#define MSGID_RUNNING_LIST                  "RUNNING_LIST" /** running app list */
#define MSGID_RUNNING_LIST_ERR              "RUNNING_LIST_ERR" /** running app list handling error */
#define MSGID_FOREGROUND_INFO               "FOREGROUND_INFO" /** foreground app info */
#define MSGID_LOADING_LIST                  "LOADING_LIST" /** loading app list */
#define MSGID_GET_FOREGROUND_APPINFO        "GET_FOREGROUND_APPINFO" /** get foreground app info */
#define MSGID_GET_FOREGROUND_APP_ERR        "GET_FOREGROUND_APP_ERR"  /** Failure to handle a request*/
#define MSGID_SPLASH_TIMEOUT                "SPLASH_TIMEOUT" /** splash timeout from LSM */
#define MSGID_APPLAUNCH_LOCK                "APPLAUNCH_LOCK" /** app launch lock status */
#define MSGID_NATIVE_APP_HANDLER            "NATIVE_APP_HANDLER" /** all list handled by native app handlers */
#define MSGID_NATIVE_APP_LIFE_CYCLE_EVENT   "NATIVE_APP_LIFE_CYCLE_EVENT" /** native app life cycle event */
#define MSGID_NATIVE_CLIENT_INFO            "NATIVE_CLIENT_INFO"
#define MSGID_HANDLE_CRIU                   "HANDLE_CRIU"

/* app package */
#define MSGID_START_SCAN                    "START_SCAN" /** START SCANNING with locale info */
#define MSGID_APP_SCANNER                   "APP_SCANNER"
#define MSGID_APPSCAN_FAIL                  "APP_SCAN_FAIL" /** Failed to scan an application */
#define MSGID_PACKAGE_LOAD                  "PACKAGE_LOAD" /** package load flow */
#define MSGID_UNINSTALL_APP                 "UNINSTALL_APP" /** uninstall app */
#define MSGID_UNINSTALL_APP_ERR             "UNINSTALL_APP_ERR" /** uninstall app */
#define MSGID_SECURITY_VIOLATION            "SEC_VIOLATION" /** Application tried to claim to be an id different from it's path */
#define MSGID_FAIL_GET_SELECTED_PROPS       "FAIL_GET_SELECTED_PROPS" /** failed to get selected properties */
#define MSGID_RESERVED_FOR_PRIVILEGED       "RESERVED_FOR_PRIVILEGED" /** resource is reserved for privileged apps */
#define MSGID_PACKAGE_STATUS                "PACKAGE_STATUS"

/* asm */
#define MSGID_HANDLE_ASM                    "HANDLE_ASM" /** Handle Attached storage */
#define MSGID_GENERATE_PASSPHRASE_ERR       "GENERATE_PASSPHRASE_ERR" /** Failed to generate passphrase to mount ecryptfs */
#define MSGID_ECRYPTFS_ERR                  "ECRYPTFS_ERR" /** Error on mounting ecryptfs */
#define MSGID_SHARED_ECRYPTFS_ERR           "SHARED_ECRYPTFS_ERR" /** Error on mounting sharedecryptfs */

/* virtual app */
#define MSGID_VIRTUAL_APP_WARNING           "VIRTUAL_APP_WARNING" /** warning log when handling virtual app request */
#define MSGID_TMP_VIRTUAL_APP_LAUNCH        "TMP_VIRTUAL_APP_LAUNCH" /** launch tmp virtual app */
#define MSGID_ADD_VIRTUAL_APP               "ADD_VIRTUAL_APP" /** add virtual app */
#define MSGID_REMOVE_VIRTUAL_APP            "REMOVE_VIRTUAL_APP" /** remove virtual app */
#define MSGID_VIRTUAL_APP_INFO_CREATED      "VIRTUAL_APP_INFO_CREATED" /** created new app info for virtual app */
#define MSGID_VIRTUAL_APP_RETURN            "VIRTUAL_APP_RETURN" /** reply for virtual app request */

/* base db */
#define MSGID_DB_LOAD                       "DB_LOAD" /** db load */
#define MSGID_DB_LOAD_ERR                   "DB_LOAD_ERR" /** db load error */

/* launch point manager */
#define MSGID_LAUNCH_POINT_REQUEST          "LAUNCH_POINT_REQUEST"
#define MSGID_LAUNCH_POINT                  "LAUNCH_POINT" /** launch point */
#define MSGID_LAUNCH_POINT_WARNING          "LAUNCH_POINT_WARNING" /** launch point warning */
#define MSGID_LAUNCH_POINT_ERROR            "LAUNCH_POINT_ERROR" /** launch point error */
#define MSGID_LAUNCH_POINT_ACTION           "LAUNCH_POINT_ACTION" /** launch point action*/
#define MSGID_LAUNCH_POINT_ADDED            "LAUNCH_POINT_ADDED" /** adding launch point done */
#define MSGID_LAUNCH_POINT_UPDATED          "LAUNCH_POINT_UPDATED" /** updating launch point Done*/
#define MSGID_LAUNCH_POINT_REMOVED          "LAUNCH_POINT_REMOVED" /** removing launch point Done*/
#define MSGID_LAUNCH_POINT_MOVED            "LAUNCH_POINT_MOVED" /** moving launch point Done*/
#define MSGID_LAUNCH_POINT_DB_HANDLE        "LAUNCH_POINT_DB_HANDLE" /**  launch point db handling */
#define MSGID_LAUNCH_POINT_REPLY_SUBSCRIBER "LAUNCH_POINT_REPLY_SUBSCRIBER" /** reply launch point info to subscriber */

/* ls-service */
#define MSGID_LUNA_SUBSCRIPTION             "LUNA_SUBSCRIPTION" /** luna subscription */
#define MSGID_SUBSCRIPTION_REPLY            "SUBSCRIPTION_REPLY" /** subscription reply */
#define MSGID_SUBSCRIPTION_REPLY_ERR        "SUBSCRIPTION_REPLY_ERR" /** subscription reply err */
#define MSGID_LSCALL_ERR                    "LSCALL_ERR" /** ls call error */
#define MSGID_LSCALL_RETURN_FAIL            "LSCALL_RETURN_FAIL" /** ls call return fail */
#define MSGID_GENERAL_JSON_PARSING_ERROR    "GENERAL_JSON_PARSING_ERROR" /** General luna bus related error log */
#define MSGID_DEPRECATED_API                "DEPRECATED_API" /** Call of a deprecated api */
#define MSGID_SRVC_REGISTER_FAIL            "SRVC_REGISTER_FAIL" /** Failure to register for ls2 bus */
#define MSGID_SRVC_CATEGORY_FAIL            "SRVC_CATEGORY_FAIL" /** Failure to register category for ls2 bus */
#define MSGID_SRVC_ATTACH_FAIL              "SRVC_ATTACH_FAIL" /** Failure to attach to service */
#define MSGID_SRVC_DETACH_FAIL              "SRVC_DETACH_FAIL" /** Failure to detach from service */
#define MSGID_OBJECT_CASTING_FAIL           "OBJECT_CASTING_FAIL" /** Failure to cast object */

/* notification */
#define MSGID_NOTIFICATION                  "NOTIFICATION" /** error about loc string **/
#define MSGID_NOTIFICATION_FAIL             "NOTIFICATION_FAIL" /** error about fail to send to notiservice **/

/* call chain */
#define MSGID_CALLCHAIN_ERR                 "CALLCHAIN_ERR" /* error about callchain **/

#define MSGID_TV_CONFIGURATION                  "TV_CONFIGURATION"
#define MSGID_GET_LASTINPUT_FAIL                "GET_LASTINPUT_FAIL" /** Failed to request getLastInput to tvservice */
#define MSGID_LAUNCH_LASTINPUT_ERR              "LAUNCH_LASTINPUT_ERR" /** error about launching last input app */
#define MSGID_APPSCAN_FILTER_TV                 "APPSCAN_FILTER_TV"
#define MSGID_CLEAR_FIRST_LAUNCHING_APP         "CLEAR_FIRST_LAUNCHNIG_APP" /** cancel launching first app on foreground app change */

/* package */
#define MSGID_PER_APP_SETTINGS                  "PER_APP_SETTINGS" /** per app settings feature */

/* launch point */
#define MSGID_LAUNCH_POINT_ORDERING             "LAUNCH_POINT_ORDERING" /* Ordering Handling */
#define MSGID_LAUNCH_POINT_ORDERING_WARNING     "LAUNCH_POINT_ORDERING_WARNING" /* Ordering Warning */
#define MSGID_LAUNCH_POINT_ORDERING_ERROR       "LAUNCH_POINT_ORDERING_ERROR" /* Ordering Error */

/* customization */
#define MSGID_LOAD_ICONS_FAIL                   "LOAD_ICONS_FAIL" /** fail to load icons info */


#define API_ERR_CODE_GENERAL                    1
#define API_ERR_CODE_INVALID_PAYLOAD            2
#define API_ERR_CODE_DEPRECATED                 999

PmLogContext GetSAMPmLogContext();

#endif // LOGGING_H

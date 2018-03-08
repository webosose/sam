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

#ifndef CORE_BUS_LUNASERVICE_API_H_
#define CORE_BUS_LUNASERVICE_API_H_

// category
#define API_CATEGORY_GENERAL  "/"
#define API_CATEGORY_DEV      "/dev"

// core API
// core API: lifecycle
#define API_LAUNCH                              "launch"
#define API_OPEN                                "open"
#define API_PAUSE                               "pause"
#define API_CLOSE_BY_APPID                      "closeByAppId"
#define API_CLOSE_ALL_APPS                      "closeAllApps"
#define API_CLOSE                               "close" // deprecated
#define API_RUNNING                             "running"
#define API_CHANGE_RUNNING_APPID                "changeRunningAppId"
#define API_GET_APP_LIFE_EVENTS                 "getAppLifeEvents"
#define API_GET_APP_LIFE_STATUS                 "getAppLifeStatus"
#define API_GET_FOREGROUND_APPINFO              "getForegroundAppInfo"
#define API_LOCK_APP                            "lockApp"
#define API_REGISTER_APP                        "registerApp"
#define API_REGISTER_NATIVE_APP                 "registerNativeApp"
#define API_NOTIFY_ALERT_CLOSED                 "notifyAlertClosed"
#define API_NOTIFY_SPLASH_TIMEOUT               "notifySplashTimeout" // deprecated
#define API_ON_LAUNCH                           "onLaunch"  // deprecated

// core API: package
#define API_LIST_APPS                           "listApps"
#define API_GET_APP_STATUS                      "getAppStatus"
#define API_GET_APP_INFO                        "getAppInfo"
#define API_GET_APP_BASE_PATH                   "getAppBasePath"
#define API_LAUNCH_VIRTUAL_APP                  "launchVirtualApp"
#define API_ADD_VIRTUAL_APP                     "addVirtualApp"
#define API_REMOVE_VIRTUAL_APP                  "removeVirtualApp"

#define API_REGISTER_VERBS_FOR_REDIRECT         "registerVerbsForRedirect"
#define API_REGISTER_VERBS_FOR_RESOURCE         "registerVerbsForResource"
#define API_GET_HANDLER_FOR_EXTENSION           "getHandlerForExtension"
#define API_LIST_EXTENSION_MAP                  "listExtensionMap"
#define API_MIME_TYPE_FOR_EXTENSION             "mimeTypeForExtension"
#define API_GET_HANDLER_FOR_MIME_TYPE           "getHandlerForMimeType"
#define API_GET_HANDLER_FOR_MIME_TYPE_BY_VERB   "getHandlerForMimeTypeByVerb"
#define API_GET_HANDLER_FOR_URL                 "getHandlerForUrl"
#define API_GET_HANDLER_FOR_URL_BY_VERB         "getHandlerForUrlByVerb"
#define API_LIST_ALL_HANDLERS_FOR_MIME          "listAllHandlersForMime"
#define API_LIST_ALL_HANDLERS_FOR_MIME_BY_VERB  "listAllHandlersForMimeByVerb"
#define API_LIST_ALL_HANDLERS_FOR_URL           "listAllHandlersForUrl"
#define API_LIST_ALL_HANDLERS_FOR_URL_BY_VERB   "listAllHandlersForUrlByVerb"
#define API_LIST_ALL_HANDLERS_FOR_URL_PATTERN   "listAllHandlersForUrlPattern"
#define API_LIST_ALL_HANDLERS_FOR_MULTIPLE_MIME "listAllHandlersForMultipleMime"
#define API_LIST_ALL_HANDLERS_FOR_MULTIPLE_URL_PATTERN  "listAllHandlersForMultipleUrlPattern"

// core API: launchpoint
#define API_ADD_LAUNCHPOINT                     "addLaunchPoint"
#define API_UPDATE_LAUNCHPOINT                  "updateLaunchPoint"
#define API_REMOVE_LAUNCHPOINT                  "removeLaunchPoint"
#define API_MOVE_LAUNCHPOINT                    "moveLaunchPoint"
#define API_LIST_LAUNCHPOINTS                   "listLaunchPoints"
#define API_SEARCH_APPS                         "searchApps"  // this should move into package for later

//---------------------------------------------------------------------------
// [ Error code list ]
//---------------------------------------------------------------------------
// common error code
#define API_ERR_CODE_GENERAL          1
#define API_ERR_CODE_INVALID_PAYLOAD  2
#define API_ERR_CODE_DEPRECATED       999

// For core API : lifecycle

// For core API : package

// For core API : launchpoint


#endif  // CORE_BUS_LUNASERVICE_API_H_


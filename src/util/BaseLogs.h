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
#ifndef BASE_LOGS_H
#define BASE_LOGS_H

#include <util/Logging.h>

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

#endif  // BASE_LOGS_H


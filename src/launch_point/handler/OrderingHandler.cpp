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

#include <launch_point/handler/OrderingHandler.h>

void OrderingHandler::Init()
{
    //virtual
}

/***********************************************************/
/** handling DB (handle/reload/update) *********************/
/***********************************************************/
void OrderingHandler::HandleDbState(bool connection)
{
    //virtual
}

void OrderingHandler::ReloadDbData(bool connection)
{
    //virtual
}

/*******************************************************/
/** handling ordered list (set/update/visibleLPs) ******/
/*******************************************************/
void OrderingHandler::MakeLaunchPointsInOrder(const std::vector<LaunchPointPtr>& visible_lps, const pbnjson::JValue& changed_reason)
{
    //virtual
}

/*******************************************************/
/*** handling data (insert/update/delete with DB) ******/
/*******************************************************/
int OrderingHandler::InsertLpInOrder(const std::string& lp_id, const pbnjson::JValue& data, int position)
{
    //virtual
    return DEFAULT_POSITION_INVALID;
}

int OrderingHandler::UpdateLpInOrder(const std::string& lp_id, const pbnjson::JValue& data, int position)
{
    //virtual
    return DEFAULT_POSITION_INVALID;
}

void OrderingHandler::DeleteLpInOrder(const std::string& lp_id)
{
    //virtual
}

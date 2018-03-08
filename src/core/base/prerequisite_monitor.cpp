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

#include "core/base/prerequisite_monitor.h"

#include "core/base/logging.h"


PrerequisiteItem::PrerequisiteItem() : status_(PrerequisiteItemStatus::Ready) {
  LOG_DEBUG("[PrerequisiteItem] Created");
}

PrerequisiteItem::~PrerequisiteItem() {
  LOG_DEBUG("[PrerequisiteItem] Released");
}

void PrerequisiteItem::SetStatus(PrerequisiteItemStatus status) {
  switch(status) {
    case PrerequisiteItemStatus::Ready:
    case PrerequisiteItemStatus::Doing:
      return;
      break;
    default:
      break;
  }
  status_ = status;
  if(status_notifier_) status_notifier_(status_);
}

////////////////////////////////////
// Prerequisite Monitor
////////////////////////////////////
PrerequisiteMonitor::~PrerequisiteMonitor() {
  LOG_DEBUG("[PrerequisiteMonitor] Released in destructor");
}

PrerequisiteMonitor& PrerequisiteMonitor::Create(std::function<void(PrerequisiteResult)> result_handler) {
  PrerequisiteMonitor* monitor = new PrerequisiteMonitor(result_handler);
  return *monitor;
}

void PrerequisiteMonitor::AddItem(PrerequisiteItemPtr item)  {
  if(item == nullptr) return;

  LOG_DEBUG("[PrerequisiteMonitor] Item Added");
  item->status_notifier_ = std::bind(&PrerequisiteMonitor::HandleItemStatus, this, std::placeholders::_1);
  prerequisites_.push_back(item);
}

void PrerequisiteMonitor::Run() {
  if(prerequisites_.empty()) {
    if(result_handler_ != nullptr) result_handler_(PrerequisiteResult::Passed);
    return;
  }

  for(auto item: prerequisites_) {
    LOG_DEBUG("[PrerequisiteMonitor] Item Starting");
    item->status_ = PrerequisiteItemStatus::Doing;
    item->Start();
  }
}

void PrerequisiteMonitor::HandleItemStatus(PrerequisiteItemStatus status) {

  LOG_DEBUG("[PrerequisiteMonitor] Item Status Changed: %d", (int) status);

  PrerequisiteResult result = PrerequisiteResult::Passed;

  for(auto item: prerequisites_) {
    LOG_DEBUG("item checking: %d", (int) item->Status());
    switch(item->Status()) {
      case PrerequisiteItemStatus::Ready:
      case PrerequisiteItemStatus::Doing:
        return;
        break;
      case PrerequisiteItemStatus::Failed:
        result = PrerequisiteResult::Failed;
        break;
      default:
        break;
    }
  }

  LOG_DEBUG("[PrerequisiteMonitor] All Item Ready. Now call result handler");
  result_handler_(result);

  LOG_DEBUG("[PrerequisiteMonitor] Done all duty. Now Releasing itself");
  prerequisites_.clear();

  // TODO: PrerequisiteMonitor doesn't guarantee item's timing issue
  //       Currently it focuses on high level multi condition checking before doing something
  //       Please use this after understanding full flows
  //       Improving internal common libraries for more general uses is always welcome
  // g_timeout_add(0, AsyncDelete, (gpointer)this);

  delete this;
}

// For later use in case when it needs
gboolean PrerequisiteMonitor::AsyncDelete(gpointer data) {
  PrerequisiteMonitor *p = static_cast<PrerequisiteMonitor*>(data);
  if (!p) return FALSE;

  p->prerequisites_.clear();

  delete p;
  return FALSE;
}

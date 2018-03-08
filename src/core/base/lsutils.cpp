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

#include "core/base/lsutils.h"

LSDelayedMessage& LSDelayedMessage::acquire(LSMessage *message) {
  LSDelayedMessage *msg = new LSDelayedMessage(message);
  return *msg;
}

gboolean LSDelayedMessage::cbAsyncDelete(gpointer data) {
  LSDelayedMessage *p = reinterpret_cast<LSDelayedMessage*>(data);
  if (!p) return false;
  delete p;
  return false;
}

LSDelayedMessage::LSDelayedMessage(LSMessage *message) : m_message(message) {
  LSMessageRef(m_message);
}

LSDelayedMessage::~LSDelayedMessage() {
  LSMessageUnref(m_message);
}

void LSDelayedMessage::setCondition(boost::signal<void()> &condition) {
  m_connection = condition.connect(
      std::bind(static_cast<void(LSDelayedMessage::*)()>(&LSDelayedMessage::onCondition), this));
}

void LSDelayedMessage::setCondition(boost::signal<void(bool)> &condition) {
  m_connection = condition.connect(
      std::bind(static_cast<void (LSDelayedMessage::*)(bool)>(&LSDelayedMessage::onCondition), this, std::placeholders::_1));
}

void LSDelayedMessage::onCondition() {
  m_action(m_message);
  m_connection.disconnect();
  g_timeout_add(0, cbAsyncDelete, (gpointer)this);
}

void LSDelayedMessage::onCondition(bool success) {
  m_action(m_message);
  m_connection.disconnect();
  g_timeout_add(0, cbAsyncDelete, (gpointer)this);
}

void LSDelayedMessage::setAction(std::function<bool(LSMessage*)> action) {
  m_action = action;
}

std::string GetCallerFromMessage(LSMessage* message) {
  std::string caller = "";
  if (!message) return "";

  if (LSMessageGetApplicationID(message))
    caller = LSMessageGetApplicationID(message);
  else if (LSMessageGetSenderServiceName(message))
    caller = LSMessageGetSenderServiceName(message);
  return caller;
}

std::string GetCallerID(const std::string& caller) {
  if (std::string::npos == caller.find(" ")) return caller;
  return caller.substr(0, caller.find(" "));
}

std::string GetCallerPID(const std::string& caller) {
  if (std::string::npos == caller.find(" ")) return "";
  return caller.substr(caller.find(" ")+1);
}

std::string GetCategoryFromMessage(LSMessage* message) {
  if (!message) return std::string("");
  const char* category = LSMessageGetCategory(message);
  if (!category) return std::string("");
  return std::string(category);
}

std::string GetMethodFromMessage(LSMessage* message) {
  if (!message) return "";
  const char* method = LSMessageGetMethod(message);
  if (!method) return "";
  return std::string(method);
}


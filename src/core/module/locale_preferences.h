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

#ifndef CORE_MODULE_LOCALE_PREFERENCES_H_
#define CORE_MODULE_LOCALE_PREFERENCES_H_

#include <string>

#include <luna-service2/lunaservice.h>
#include <boost/signals2.hpp>
#include <pbnjson.hpp>

#include "core/base/singleton.h"

class LocalePreferences : public Singleton<LocalePreferences> {
 public:
  void Init();
  void OnRestInit();

  const std::string& UIlocale() const { return locale_info_; }
  const std::string& Language() const { return language_; }
  const std::string& Script() const { return script_; }
  const std::string& Region() const { return region_; }

  boost::signals2::signal<void (std::string, std::string, std::string)> signalLocaleChanged;

 private:
  friend class Singleton<LocalePreferences>;

  LocalePreferences();
  ~LocalePreferences();

  void OnSettingServiceStatusChanaged(bool connection);
  static bool OnLocaleInfoReceived(LSHandle *sh, LSMessage *message, void *user_data);

  void UpdateLocaleInfo(const pbnjson::JValue& j_locale);
  void SetLocaleInfo(const std::string& locale);

  std::string locale_info_;
  std::string language_;
  std::string script_;
  std::string region_;

  LSMessageToken locale_info_token_;
};
#endif // CORE_MODULE_LOCALE_PREFERENCES_H_


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

#include <core/util/singleton.h>
#include <luna-service2/lunaservice.h>
#include <webosi18n.h>
#include <memory>


class ResBundleAdaptor: public Singleton<ResBundleAdaptor> {
private:
    std::string m_resFile;
    std::string m_resPath;

    std::shared_ptr<ResBundle> m_resBundle;

    ResBundleAdaptor& operator=(const ResBundleAdaptor&);
    ResBundleAdaptor(const ResBundleAdaptor&);

    ResBundleAdaptor();
    virtual ~ResBundleAdaptor();

    friend class Singleton<ResBundleAdaptor> ;

public:
    std::string getLocString(const std::string& key);
    void setLocale(const std::string& locale);
};

class ToastHelper {
public:
    ToastHelper();
    ~ToastHelper();

    bool createToast(pbnjson::JValue msg);
    static bool toastLsCallBack(LSHandle *sh, LSMessage *message, void *user_data);
};


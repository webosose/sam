// Copyright (c) 2012-2019 LG Electronics, Inc.
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

#include <luna-service2/lunaservice.h>
#include "interface/IClassName.h"
#include "interface/ISingleton.h"
#include <webosi18n.h>
#include <memory>
#include "util/Logger.h"

class ResBundleAdaptor: public ISingleton<ResBundleAdaptor>,
                        public IClassName {
friend class ISingleton<ResBundleAdaptor> ;
private:
    std::string m_resFile;
    std::string m_resPath;

    std::shared_ptr<ResBundle> m_resBundle;

    ResBundleAdaptor& operator=(const ResBundleAdaptor&);
    ResBundleAdaptor(const ResBundleAdaptor&);

    ResBundleAdaptor();
    virtual ~ResBundleAdaptor();

public:
    std::string getLocString(const std::string& key);
    void setLocale(const std::string& locale);
};

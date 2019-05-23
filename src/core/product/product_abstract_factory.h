// Copyright (c) 2016-2018 LG Electronics, Inc.
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

#ifndef CORE_BASE_PRODUCT_ABSTRACT_FACTORY_H_
#define CORE_BASE_PRODUCT_ABSTRACT_FACTORY_H_

#include "core/base/prerequisite_monitor.h"
#include "core/base/singleton.h"

class ProductAbstractFactory: public Singleton<ProductAbstractFactory> {
public:
    ProductAbstractFactory();
    ~ProductAbstractFactory();

    void Initialize(PrerequisiteMonitor& prerequisite_monitor);
    void OnReady();
    void Terminate();

private:
    friend class Singleton<ProductAbstractFactory> ;
};

#endif  // CORE_BASE_PRODUCT_ABSTRACT_FACTORY_H_

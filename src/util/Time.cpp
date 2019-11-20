// Copyright (c) 2019 LG Electronics, Inc.
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

#include "Time.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/lexical_cast.hpp>

const double NANO_SECOND = 1000000000;

Time::Time()
{
}

Time::~Time()
{
}


double Time::getCurrentTime()
{
    timespec current_time;
    if (clock_gettime(CLOCK_MONOTONIC, &current_time) == -1)
        return -1;
    return (double) current_time.tv_sec * NANO_SECOND + (double) current_time.tv_nsec;
}

string Time::generateUid()
{
    boost::uuids::uuid uid = boost::uuids::random_generator()();
    return string(boost::lexical_cast<string>(uid));
}

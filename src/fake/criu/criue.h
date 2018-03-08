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

#ifndef FAKE_CRIUE_H_
#define FAKE_CRIUE_H_

#include <iostream>

bool criue_check_images(const char* fork_params)
{
  return false;
}

int criue_restore_app(const std::string& id, int argc, char **fork_params)
{
  return -1;
}

#endif /* FAKE_CRIUE_H_ */

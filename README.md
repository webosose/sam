Summary
-------
SAM is responsible for systems and application management

Description
-----------

SAM is responsible for
* Managing the running of applications
* Managing the installation and removal of applications

Dependencies
---------------------

Below are the tools and libraries required to build SAM:

* cmake
* gcc
* glib-2.0
* make
* pkg-config
* Boost library (signals and regex)
* libicu-dev
* webosose/cmake-modules-webos
* webosose/pmloglib
* webosose/libpbnjson
* webosose/luna-service2

## Building

Once you have downloaded the source, enter the following to build it (after
changing into the directory under which it was downloaded):

    $ mkdir BUILD
    $ cd BUILD
    $ cmake ..
    $ make

Copyright and License Information
=================================
Unless otherwise specified, all content, including all source code files and
documentation files in this repository are:

Copyright (c) 2012-2018 LG Electronics, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 SPDX-License-Identifier: Apache-2.0

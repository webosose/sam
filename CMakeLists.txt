# Copyright (c) 2012-2019 LG Electronics, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

#
# sam/CMakeLists.txt
#

cmake_minimum_required(VERSION 2.8.7)

project(sam C CXX)

include(webOS/webOS)

webos_modules_init(1 0 0 QUALIFIER RC4)
webos_component(2 0 0)
webos_distro_dep()
webos_add_compiler_flags(ALL -Wall -std=gnu++0x)
webos_add_compiler_flags(ALL -DBOOST_SIGNALS_NO_DEPRECATION_WARNING)

add_definitions(-DLOGGER_ENABLED)

include(FindPkgConfig)

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${GLIB2_CFLAGS_OTHER})

pkg_check_modules(GIO2 REQUIRED gio-2.0)
include_directories(${GIO2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${GIO2_CFLAGS_OTHER})

pkg_check_modules(LUNASERVICE2 REQUIRED luna-service2)
include_directories(${LUNASERVICE2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${LUNASERVICE2_CFLAGS_OTHER})

pkg_check_modules(LUNASERVICE2CPP REQUIRED luna-service2++)
include_directories(${LUNASERVICE2CPP_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${LUNASERVICE2CPP_CFLAGS_OTHER})

pkg_check_modules(PBNJSON_C REQUIRED pbnjson_c)
include_directories(${PBNJSON_C_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${PBNJSON_C_CFLAGS_OTHER})

pkg_check_modules(PBNJSON_CPP REQUIRED pbnjson_cpp)
include_directories(${PBNJSON_CPP_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${PBNJSON_CPP_CFLAGS_OTHER})

pkg_check_modules(PMLOG REQUIRED PmLogLib)
include_directories(${PMLOG_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${PMLOG_CFLAGS_OTHER})

find_package(Boost REQUIRED COMPONENTS regex system filesystem)
include_directories(${Boost_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${Boost_CFLAGS_OTHER})

pkg_check_modules(PROCPS REQUIRED libprocps)
include_directories(PROCPS_INCLUDE_DIRS)
webos_add_compiler_flags(ALL ${PROCPS_CFLAGS})

find_library(ICU NAMES icuuc)
if(ICU STREQUAL "ICU-NOTFOUND")
   message(FATAL_ERROR "Failed to find ICU4C libraries. Please install.")
endif()

find_library(RT NAMES rt)
if(RT STREQUAL "RT-NOTFOUND")
   message(FATAL_ERROR "Failed to find rt libraries. Please link.")
endif()

# Require that all undefined symbols are satisfied by the libraries from target_link_libraries()
webos_add_linker_options(ALL --no-undefined)

# Build the sam executable
file(GLOB_RECURSE SOURCES src/*.cpp)

webos_configure_source_files(cfg ${PROJECT_SOURCE_DIR}/src/Environment.h)

include_directories(src)
include_directories(${PROJECT_BINARY_DIR}/Configured/src)
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

set(LIBS
    ${GLIB2_LDFLAGS}
    ${GIO2_LDFLAGS}
    ${PBNJSON_C_LDFLAGS}
    ${PBNJSON_CPP_LDFLAGS}
    ${LUNASERVICE2_LDFLAGS}
    ${LUNASERVICE2CPP_LDFLAGS}
    ${PMLOG_LDFLAGS}
    ${Boost_LIBRARIES}
    ${ICU}
    ${RT}
    ${PROCPS_LDFLAGS})
target_link_libraries(${CMAKE_PROJECT_NAME} ${LIBS})

webos_build_daemon(RESTRICTED_PERMISSIONS)
webos_build_system_bus_files()

file(GLOB_RECURSE SCHEMAS files/schema/*.schema)
install(FILES ${SCHEMAS} DESTINATION ${WEBOS_INSTALL_WEBOS_SYSCONFDIR}/schemas/sam)

# sam conf files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/files/conf/sam-conf.json.in ${CMAKE_CURRENT_BINARY_DIR}/files/conf/sam-conf.json)

file(GLOB_RECURSE SAM_CONF_FILES ${CMAKE_CURRENT_BINARY_DIR}/files/conf/*)
install(FILES ${SAM_CONF_FILES} DESTINATION ${WEBOS_INSTALL_WEBOS_SYSCONFDIR})

webos_config_build_doxygen(doc Doxyfile)

# SPDX-FileCopyrightText: 2026 techoraye
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Raye widget style and window decoration. The code under kstyle/,
# kdecoration/ and libraye-common/ starts from KDE Breeze (version in
# UPSTREAM); this file replaces Breeze's top level and builds only
# those parts, for Qt 6.

cmake_minimum_required(VERSION 3.16)
project(raye-style LANGUAGES CXX)

set(KF6_MIN_VERSION "6.22.0")
set(QT_MIN_VERSION "6.10.0")
set(KDE_COMPILERSETTINGS_LEVEL "5.82")
# the kstyle/ and kdecoration/ CMake files are written for both Qt majors
set(QT_MAJOR_VERSION 6)

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_NO_CREATE_VERSIONLESS_TARGETS ON)
set(QT_NO_CREATE_VERSIONLESS_FUNCTIONS ON)

include(KDEInstallDirs6)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)
include(ECMInstallIcons)
include(ECMQtDeclareLoggingCategory)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED CONFIG COMPONENTS Core DBus Gui Svg Widgets)
find_package(Qt6 ${QT_MIN_VERSION} CONFIG OPTIONAL_COMPONENTS Quick)

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    ColorScheme
    Config
    CoreAddons
    GuiAddons
    I18n
    IconThemes
    KCMUtils
    WindowSystem)
# Breeze can build without KStyle; Raye always ships with it so both
# behave the same in KDE applications.
find_package(KF6FrameworkIntegration ${KF6_MIN_VERSION} REQUIRED CONFIG)
find_package(KDecoration3 REQUIRED)
if(Qt6Quick_FOUND)
    find_package(KF6KirigamiPlatform ${KF6_MIN_VERSION} REQUIRED)
endif()

set(HAVE_QTDBUS ${Qt6DBus_FOUND})
set(RAYE_HAVE_KSTYLE ${KF6FrameworkIntegration_FOUND})
set(RAYE_HAVE_QTQUICK ${Qt6Quick_FOUND})

# https://bugreports.qt.io/browse/QTBUG-114706
add_library(Qt::Core ALIAS Qt6::Core)
add_library(Qt::Gui ALIAS Qt6::Gui)

add_subdirectory(libraye-common)
add_subdirectory(kstyle)
add_subdirectory(kdecoration)

ki18n_install(po)

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME raye-names
        COMMAND Python3::Interpreter -I ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_names.py ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME raye-import-tools
        COMMAND Python3::Interpreter -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/cmake -p "test_*.py")
    add_subdirectory(autotests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

cmake_minimum_required(VERSION 3.16)

set(CMAKE_C_STANDARD 99)

set(PROJECT_VERSION "5.24.80")
set(PROJECT_VERSION_MAJOR 5)

set(QT_MIN_VERSION "5.15.2")
set(KF5_MIN_VERSION "5.91")
set(KDE_COMPILERSETTINGS_LEVEL "5.82")
set(WAYLAND_MIN_VERSION "1.20")

project(KWaylandServer VERSION ${PROJECT_VERSION})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ECM setup
include(FeatureSummary)
find_package(ECM ${KF5_MIN_VERSION} NO_MODULE REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(CheckIncludeFile)
include(CMakeFindFrameworks)
include(CMakePackageConfigHelpers)
include(FeatureSummary)
include(ECMGenerateExportHeader)
include(ECMSetupVersion)
include(ECMGenerateHeaders)
include(ECMQtDeclareLoggingCategory)
include(ECMPoQmTools)
include(ECMAddQch)
include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)
include(KDEClangFormat)
include(KDEGitCommitHooks)

option(BUILD_QCH "Build API documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)" OFF)
add_feature_info(QCH ${BUILD_QCH} "API documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)")

ecm_setup_version(PROJECT VARIABLE_PREFIX KWAYLANDSERVER
                        VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kwaylandserver_version.h"
                        PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KWaylandServerConfigVersion.cmake"
                        SOVERSION 5)

# Dependencies
find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED Concurrent Gui WaylandClient)
find_package(KF5Wayland ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(PlasmaWaylandProtocols 1.7.0 CONFIG REQUIRED)

if (QT_MAJOR_VERSION EQUAL "5")
    find_package(QtWaylandScanner REQUIRED)
endif()

find_package(Wayland ${WAYLAND_MIN_VERSION} COMPONENTS Client Server)
set_package_properties(Wayland PROPERTIES
                       TYPE REQUIRED
                      )

find_package(WaylandScanner)

find_package(WaylandProtocols 1.25)
set_package_properties(WaylandProtocols PROPERTIES TYPE REQUIRED)

find_package(EGL)
set_package_properties(EGL PROPERTIES TYPE REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

# adjusting CMAKE_C_FLAGS to get wayland protocols to compile
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu90")

add_definitions(-DQT_NO_FOREACH)
add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050d00)
# Subdirectories
if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/po")
    ecm_install_po_files_as_qm(po)
endif()

add_subdirectory(src)

if (BUILD_TESTING)
    add_subdirectory(autotests)
    add_subdirectory(tests)
endif()

# create a Config.cmake and a ConfigVersion.cmake file and install them
set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KWaylandServer")

if (BUILD_QCH)
    ecm_install_qch_export(
        TARGETS KWaylandServer_QCH
        FILE KWaylandServerQchTargets.cmake
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        COMPONENT Devel
    )
    set(PACKAGE_INCLUDE_QCHTARGETS "include(\"\${CMAKE_CURRENT_LIST_DIR}/KWaylandServerQchTargets.cmake\")")
endif()

configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/KWaylandServerConfig.cmake.in"
                              "${CMAKE_CURRENT_BINARY_DIR}/KWaylandServerConfig.cmake"
                              INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
                              )

install(FILES  "${CMAKE_CURRENT_BINARY_DIR}/KWaylandServerConfig.cmake"
               "${CMAKE_CURRENT_BINARY_DIR}/KWaylandServerConfigVersion.cmake"
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        COMPONENT Devel )

install(EXPORT KWaylandServerTargets DESTINATION "${CMAKECONFIG_INSTALL_DIR}" FILE KWaylandServerTargets.cmake NAMESPACE Plasma:: )


install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kwaylandserver_version.h
        DESTINATION ${KDE_INSTALL_INCLUDEDIR} COMPONENT Devel )

# add clang-format target for all our real source files
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

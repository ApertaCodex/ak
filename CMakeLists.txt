cmake_minimum_required(VERSION 3.20)

project(ak
  VERSION 4.2.28
  DESCRIPTION "ak — API Key Manager"
  HOMEPAGE_URL "https://github.com/apertacodex/ak"
  LANGUAGES CXX)

# ---- Basics ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(BUILD_TESTING "Build tests" ON)

# ---- Version (git describe -> compile define) ----
find_package(Git QUIET)
set(GIT_DESCRIBE "${PROJECT_VERSION}") # fallback "4.2.28"
if(Git_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --always
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
add_compile_definitions(AK_VERSION_STRING="${GIT_DESCRIBE}")

# ---- Debian-safe version for packages ----
# v4.2.28-3-gabc1234[-dirty] -> 4.2.28+3.gabc1234[+dirty]
set(_AK_RAW "${GIT_DESCRIBE}")
string(REGEX REPLACE "^v" "" _AK_STRIPPED "${_AK_RAW}")
if(_AK_STRIPPED MATCHES "^([0-9][A-Za-z0-9.+~-]*)-([0-9]+)-g([0-9a-f]+)(-dirty)?$")
  set(_AK_TAG "${CMAKE_MATCH_1}")
  set(_AK_CNT "${CMAKE_MATCH_2}")
  set(_AK_SHA "${CMAKE_MATCH_3}")
  set(_AK_DIRTY "${CMAKE_MATCH_4}")
  set(AK_DEBVER "${_AK_TAG}+${_AK_CNT}.g${_AK_SHA}")
  if(_AK_DIRTY)
    set(AK_DEBVER "${AK_DEBVER}+dirty")
  endif()
else()
  set(AK_DEBVER "${_AK_STRIPPED}")
endif()
message(STATUS "Git describe: ${GIT_DESCRIBE}  ->  Debian-safe: ${AK_DEBVER}")

# ---- Dependencies ----
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(Threads REQUIRED)
find_package(OpenSSL QUIET)
find_package(ZLIB QUIET)

# ---- Sources ----
file(GLOB_RECURSE AK_HEADERS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/include/**/*.hpp")
file(GLOB_RECURSE AK_ALL_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/**/*.cpp")
set(AK_APP_MAIN "${CMAKE_SOURCE_DIR}/src/main.cpp")
list(REMOVE_ITEM AK_ALL_SOURCES "${AK_APP_MAIN}")

# ---- Core library (all code except main) ----
add_library(ak_core STATIC ${AK_ALL_SOURCES} ${AK_HEADERS})
set_target_properties(ak_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(ak_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(ak_core
  PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network # PUBLIC so exe shows Qt deps
  PRIVATE Threads::Threads
)
if(OpenSSL_FOUND)
  target_link_libraries(ak_core PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()
if(ZLIB_FOUND)
  target_link_libraries(ak_core PRIVATE ZLIB::ZLIB)
endif()

if(MSVC)
  target_compile_options(ak_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(ak_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---- Executable ----
add_executable(ak ${AK_APP_MAIN})
target_link_libraries(ak PRIVATE ak_core)

# Portable RPATH
if(APPLE)
  set_target_properties(ak PROPERTIES
    INSTALL_RPATH "@loader_path/../Frameworks;@executable_path/../Frameworks")
elseif(UNIX AND NOT APPLE)
  set_target_properties(ak PROPERTIES
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN")
endif()

# ---- Install ----
include(GNUInstallDirs)

install(TARGETS ak
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  BUNDLE DESTINATION .)

install(FILES docs/ak.1
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
  OPTIONAL)

install(FILES ak.desktop
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
  OPTIONAL)

install(FILES logo.png
  DESTINATION ${CMAKE_INSTALL_DATAROOT_DIR}/pixmaps
  RENAME ak.png
  OPTIONAL)

install(FILES logo.svg
  DESTINATION ${CMAKE_INSTALL_DATAROOT_DIR}/icons/hicolor/scalable/apps
  RENAME ak.svg
  OPTIONAL)

# ---- Tests ----
include(CTest)
if(BUILD_TESTING)
  enable_testing()
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()
  file(GLOB_RECURSE AK_TESTS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*.cpp")
  add_executable(ak_tests ${AK_TESTS})
  target_link_libraries(ak_tests PRIVATE ak_core GTest::gtest GTest::gtest_main)
  add_test(NAME ak_tests COMMAND ak_tests)
endif()

# ---- Packaging ----
# Respect -DCPACK_PACKAGE_VERSION if provided; otherwise use AK_DEBVER.
if(NOT DEFINED CPACK_PACKAGE_VERSION OR CPACK_PACKAGE_VERSION STREQUAL "")
  set(CPACK_PACKAGE_VERSION "${AK_DEBVER}")
endif()

set(CPACK_PACKAGE_NAME "ak")
set(CPACK_PACKAGE_VENDOR "ApertaCodex")
set(CPACK_PACKAGE_CONTACT "support@apertacodex.ai")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "ak — API Key Manager")

if(WIN32)
  set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
  set(CPACK_GENERATOR "DragNDrop;TGZ")
  set(CPACK_DMG_VOLUME_NAME "ak-${CPACK_PACKAGE_VERSION}")
else()
  set(CPACK_GENERATOR "DEB;TGZ")
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "ApertaCodex <support@apertacodex.ai>")
  set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON) # auto runtime deps
endif()

include(CPack)

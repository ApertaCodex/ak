cmake_minimum_required(VERSION 3.16)

# -----------------------------
# Project & global configuration
# -----------------------------
project(ak
  VERSION 4.2.28
  DESCRIPTION "Cross-platform API Key Manager"
  HOMEPAGE_URL "https://github.com/apertacodex/ak"
  LANGUAGES CXX)

# C++ standard & policies
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
cmake_policy(SET CMP0077 NEW)
cmake_policy(SET CMP0091 NEW) # MSVC runtime selection if needed

# Options
option(AK_BUILD_GUI            "Build Qt6 GUI alongside CLI" ON)
option(AK_ENABLE_LTO           "Enable link-time optimization in Release" ON)
option(AK_ENABLE_COVERAGE      "Enable coverage flags in Debug" OFF)
option(AK_PORTABLE_RPATH       "Embed RPATH on Unix for portable installs" ON)
option(BUILD_TESTING           "Build tests" ON)  # CTest standard option
option(AK_FETCH_GTEST          "Fetch GoogleTest if not found" ON)

# -----------------------------
# Version from git, if available
# -----------------------------
set(AK_GIT_DESCRIBE "0.0.0" CACHE STRING "git describe fallback")
find_package(Git QUIET)
if(Git_FOUND)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" describe --tags --dirty --always
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE AK_GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
endif()
message(STATUS "ak version (git): ${AK_GIT_DESCRIBE}")

# expose to code
add_compile_definitions(AK_VERSION_STRING="${AK_GIT_DESCRIBE}")

# -----------------------------
# Dependencies
# -----------------------------
find_package(Threads REQUIRED)
find_package(OpenSSL QUIET)  # optional but recommended for crypto
find_package(ZLIB QUIET)

# Qt6 (GUI optional)
if(AK_BUILD_GUI)
  find_package(Qt6 QUIET COMPONENTS Widgets Network Gui Core)
  if(NOT Qt6_FOUND)
    message(WARNING "AK_BUILD_GUI=ON but Qt6 not found -> GUI disabled")
    set(AK_BUILD_GUI OFF)
  endif()
endif()

# -----------------------------
# Sources
# -----------------------------
# headers live under include/, sources under src/
file(GLOB_RECURSE AK_HEADERS
  CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/include/**/*.hpp")
file(GLOB_RECURSE AK_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/**/*.cpp")

# -----------------------------
# Main executable target
# -----------------------------
add_executable(ak ${AK_SOURCES} ${AK_HEADERS})

target_include_directories(ak
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(ak
  PRIVATE
    Threads::Threads
)

if(OpenSSL_FOUND)
  target_link_libraries(ak PRIVATE OpenSSL::SSL OpenSSL::Crypto)
  target_compile_definitions(ak PRIVATE AK_WITH_OPENSSL=1)
endif()

if(ZLIB_FOUND)
  target_link_libraries(ak PRIVATE ZLIB::ZLIB)
  target_compile_definitions(ak PRIVATE AK_WITH_ZLIB=1)
endif()

# Qt GUI bits (if enabled): compile GUI sources and link Qt
if(AK_BUILD_GUI)
  # let Qt's AUTOMOC/UIC/RCC help (in case you add .ui/.qrc later)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTORCC ON)
  set(CMAKE_AUTOUIC ON)
  target_compile_definitions(ak PRIVATE AK_WITH_QT=1)
  target_link_libraries(ak
    PRIVATE
      Qt6::Core
      Qt6::Gui
      Qt6::Widgets
      Qt6::Network
  )
endif()

# Warnings & LTO
if(MSVC)
  target_compile_options(ak PRIVATE /W4 /permissive-)
else()
  target_compile_options(ak PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(AK_ENABLE_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_ok OUTPUT ipo_msg)
  if(ipo_ok)
    set_property(TARGET ak PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(WARNING "IPO/LTO not supported: ${ipo_msg}")
  endif()
endif()

# Portable RPATH on Unix
if(UNIX AND NOT APPLE AND AK_PORTABLE_RPATH)
  set_target_properties(ak PROPERTIES
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN/../lib:\$ORIGIN"
  )
endif()

# -----------------------------
# Install layout
# -----------------------------
include(GNUInstallDirs)

install(TARGETS ak
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}         # Linux/macOS CLI
  BUNDLE  DESTINATION .                               # macOS .app (if ever used)
  COMPONENT Runtime)

# man page
if(EXISTS "${CMAKE_SOURCE_DIR}/docs/ak.1")
  install(FILES "${CMAKE_SOURCE_DIR}/docs/ak.1"
          DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
          COMPONENT Runtime)
endif()

# desktop entry & icons (if present)
if(EXISTS "${CMAKE_SOURCE_DIR}/ak.desktop")
  install(FILES "${CMAKE_SOURCE_DIR}/ak.desktop"
          DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
          COMPONENT Runtime)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/logo.png")
  install(FILES "${CMAKE_SOURCE_DIR}/logo.png"
          DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pixmaps
          RENAME ak.png
          COMPONENT Runtime)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/logo.svg")
  install(FILES "${CMAKE_SOURCE_DIR}/logo.svg"
          DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
          RENAME ak.svg
          COMPONENT Runtime)
endif()

# shell completions generated by the tool itself
# we install via a small post-install script that runs the just-installed binary
install(CODE [[
  message(STATUS "Generating shell completions...")
  execute_process(COMMAND "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/ak" completion bash
                  OUTPUT_FILE "${CMAKE_INSTALL_PREFIX}/share/bash-completion/completions/ak"
                  RESULT_VARIABLE _bash_res)
  execute_process(COMMAND "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/ak" completion zsh
                  OUTPUT_FILE "${CMAKE_INSTALL_PREFIX}/share/zsh/site-functions/_ak"
                  RESULT_VARIABLE _zsh_res)
  file(MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/share/fish/vendor_completions.d")
  execute_process(COMMAND "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/ak" completion fish
                  OUTPUT_FILE "${CMAKE_INSTALL_PREFIX}/share/fish/vendor_completions.d/ak.fish"
                  RESULT_VARIABLE _fish_res)
]])

# -----------------------------
# Testing
# -----------------------------
include(CTest)

if(BUILD_TESTING)
  enable_testing()

  # Try to find preinstalled GTest
  find_package(GTest QUIET)

  if(NOT GTest_FOUND AND AK_FETCH_GTEST)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
      URL_HASH SHA256=3b49b0a5f3df0b2b1b1b7b7e0c28503a2cdb6bd8f696ace5b35f0a1cf4f7b90f
    )
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    set(GTest_FOUND ON)
  endif()

  file(GLOB_RECURSE AK_TEST_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/tests/*.cpp")
  add_executable(ak_tests ${AK_TEST_SOURCES})
  target_include_directories(ak_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_link_libraries(ak_tests PRIVATE ak Threads::Threads)
  if(GTest_FOUND)
    target_link_libraries(ak_tests PRIVATE GTest::gtest GTest::gtest_main)
  endif()
  add_test(NAME ak_tests COMMAND ak_tests)

  # Coverage
  if(AK_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ak PRIVATE --coverage -O0)
    target_link_libraries(ak PRIVATE --coverage)
    target_compile_options(ak_tests PRIVATE --coverage -O0)
    target_link_libraries(ak_tests PRIVATE --coverage)
    add_custom_target(coverage
      COMMAND "${CMAKE_COMMAND}" -E env LC_ALL=C.UTF-8
              gcovr -r "${CMAKE_SOURCE_DIR}" --xml build/coverage.xml
              --html --html-details -o build/coverage_html/index.html
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
      COMMENT "Generating coverage report with gcovr")
  endif()
endif()

# -----------------------------
# Packaging (CPack)
# -----------------------------
set(CPACK_PACKAGE_NAME "ak")
set(CPACK_PACKAGE_VENDOR "ApertaCodex")
set(CPACK_PACKAGE_CONTACT "support@apertacodex.ai")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "API Key Manager CLI/GUI")
set(CPACK_PACKAGE_VERSION "${AK_GIT_DESCRIBE}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

# Generators by platform
if(WIN32)
  set(CPACK_GENERATOR "ZIP")           # add "NSIS" if you maintain an NSIS config
elseif(APPLE)
  set(CPACK_GENERATOR "DragNDrop;TGZ") # DMG + tarball
else()
  set(CPACK_GENERATOR "DEB;TGZ")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "ApertaCodex <support@apertacodex.ai>")
  set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libstdc++6, zlib1g")
endif()

include(CPack)

# -----------------------------
# Helper release/publish targets
# -----------------------------
# Hook your existing cmake helper scripts
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/version_bump.cmake")
  add_custom_target(bump-patch
    COMMAND ${CMAKE_COMMAND} -DLEVEL=PATCH -P "${CMAKE_SOURCE_DIR}/cmake/version_bump.cmake"
    COMMENT "Bump patch version and tag")
  add_custom_target(bump-minor
    COMMAND ${CMAKE_COMMAND} -DLEVEL=MINOR -P "${CMAKE_SOURCE_DIR}/cmake/version_bump.cmake"
    COMMENT "Bump minor version and tag")
  add_custom_target(bump-major
    COMMAND ${CMAKE_COMMAND} -DLEVEL=MAJOR -P "${CMAKE_SOURCE_DIR}/cmake/version_bump.cmake"
    COMMENT "Bump major version and tag")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/git_publish.cmake")
  add_custom_target(publish
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/git_publish.cmake"
    COMMENT "Publish current version (git release)")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/apt_publish.cmake")
  add_custom_target(publish-apt
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/apt_publish.cmake"
    COMMENT "Build & publish APT repo (for GitHub Pages)")
endif()

# Convenience meta-targets
add_custom_target(all_targets DEPENDS ak)

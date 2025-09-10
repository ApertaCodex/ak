cmake_minimum_required(VERSION 3.16)

# Version is managed by the build system
set(AK_VERSION "4.2.21")
project(ak VERSION ${AK_VERSION} LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Platform detection
if(WIN32)
    set(PLATFORM_NAME "Windows")
elseif(APPLE)
    set(PLATFORM_NAME "macOS")
else()
    set(PLATFORM_NAME "Linux")
endif()

# Compiler-specific flags
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pipe")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
    if(APPLE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
    endif()
endif()

# Coverage flags for GCC/Clang
if(CMAKE_BUILD_TYPE STREQUAL "Coverage" AND NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

# Include directories
include_directories(include)

# Source files
set(CORE_SOURCES
    src/core/config.cpp
    src/crypto/crypto.cpp
    src/storage/vault.cpp
    src/ui/ui.cpp
    src/system/system.cpp
    src/cli/cli.cpp
    src/services/services.cpp
    src/commands/commands.cpp
)

# ============================================================================
# GUI Configuration
# ============================================================================

option(BUILD_GUI "Build GUI support with Slint" ON)

if(BUILD_GUI)
    # Add GUI sources (excluding the Slint-generated code for now)
    set(GUI_SOURCES
        src/gui/gui.cpp
        src/gui/mainwindow.cpp
        src/gui/widgets/keymanager.cpp
        src/gui/widgets/servicetester.cpp
        src/gui/widgets/profilemanager.cpp
        src/gui/widgets/servicemanager.cpp
        src/gui/widgets/common/secureinput.cpp
        src/gui/widgets/common/dialogs.cpp
    )
    
    # Find required dependencies
    if(UNIX AND NOT APPLE)
        find_package(Fontconfig REQUIRED)
        find_package(X11 REQUIRED)
        find_package(OpenGL REQUIRED)
        find_package(Threads REQUIRED)
    endif()
    
    # Find Slint compiler
    find_program(SLINT_COMPILER slint-compiler)
    
    if(NOT SLINT_COMPILER)
        message(STATUS "Slint compiler not found in PATH")
        message(STATUS "Run the provided install-slint.sh script to install the Slint compiler")
        message(FATAL_ERROR "Slint compiler is required for GUI build. Install with ./install-slint.sh")
    endif()
    
    message(STATUS "Found Slint compiler: ${SLINT_COMPILER}")
    
    # Compile the Slint UI file directly into the build
    set(SLINT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/ui/main.slint")
    set(SLINT_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/slint_generated.cpp")
    
    # Add a custom command to compile the Slint file
    add_custom_command(
        OUTPUT ${SLINT_GENERATED}
        COMMAND ${SLINT_COMPILER} ${SLINT_FILE} -o ${SLINT_GENERATED}
        DEPENDS ${SLINT_FILE}
        COMMENT "Compiling Slint UI file"
        VERBATIM
    )
    
    # Add the generated file to sources
    list(APPEND GUI_SOURCES ${SLINT_GENERATED})
    
    # Create Slint assets directory
    set(SLINT_ASSETS_DIR "${CMAKE_CURRENT_BINARY_DIR}/slint-assets")
    file(MAKE_DIRECTORY ${SLINT_ASSETS_DIR})
    
    message(STATUS "Slint compiler found: Building with GUI support")
else()
    message(STATUS "Slint not enabled: Building CLI-only version")
    set(GUI_SOURCES "")
endif()

# Main executable
add_executable(ak
    src/main.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)

# Link dependencies if GUI is enabled
if(BUILD_GUI)
    # Include directories
    target_include_directories(ak PRIVATE include/gui)
    target_include_directories(ak PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    
    # Define BUILD_GUI and set Slint assets path
    target_compile_definitions(ak PRIVATE
        BUILD_GUI
        SLINT_ASSET_PATH="${SLINT_ASSETS_DIR}"
    )
    
    # Link system libraries based on platform
    if(UNIX AND NOT APPLE)
        target_link_libraries(ak PRIVATE
            Fontconfig::Fontconfig
            ${X11_LIBRARIES}
            ${OPENGL_LIBRARIES}
            Threads::Threads
            dl
        )
    elseif(APPLE)
        target_link_libraries(ak PRIVATE
            "-framework Cocoa"
            "-framework QuartzCore"
            "-framework Metal"
        )
    elseif(WIN32)
        target_link_libraries(ak PRIVATE
            user32
            gdi32
            opengl32
        )
    endif()
endif()

# Pass version to C++ code
target_compile_definitions(ak PRIVATE AK_VERSION_STRING="${AK_VERSION}")

# Windows-specific settings
if(WIN32)
    target_compile_definitions(ak PRIVATE WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_definitions(ak PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

# macOS-specific settings
if(APPLE)
    set_target_properties(ak PROPERTIES
        MACOSX_BUNDLE FALSE
    )
endif()

# Linux-specific settings
if(UNIX AND NOT APPLE)
    # Already handled in the GUI section
endif()

# ============================================================================
# Testing Configuration
# ============================================================================

option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
    enable_testing()
    
    # Download and build Google Test
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    
    # Test executable
    set(TEST_SOURCES
        tests/test_main_gtest.cpp
        tests/core/test_config.cpp
        tests/crypto/test_crypto.cpp
        tests/cli/test_cli.cpp
        tests/services/test_services.cpp
    )
    
    add_executable(ak_tests
        ${TEST_SOURCES}
        ${CORE_SOURCES}
        ${GUI_SOURCES}
    )
    
    # Link dependencies if GUI enabled
    if(BUILD_GUI)
        target_compile_definitions(ak_tests PRIVATE
            BUILD_GUI
            SLINT_ASSET_PATH="${SLINT_ASSETS_DIR}"
        )
        
        # Link system libraries based on platform
        if(UNIX AND NOT APPLE)
            target_link_libraries(ak_tests PRIVATE
                Fontconfig::Fontconfig
                ${X11_LIBRARIES}
                ${OPENGL_LIBRARIES}
                Threads::Threads
                dl
            )
        elseif(APPLE)
            target_link_libraries(ak_tests PRIVATE
                "-framework Cocoa"
                "-framework QuartzCore"
                "-framework Metal"
            )
        elseif(WIN32)
            target_link_libraries(ak_tests PRIVATE
                user32
                gdi32
                opengl32
            )
        endif()
    endif()
    
    # Pass version to test target
    target_compile_definitions(ak_tests PRIVATE AK_VERSION_STRING="${AK_VERSION}")
    
    target_include_directories(ak_tests PRIVATE tests)
    target_link_libraries(ak_tests PRIVATE gtest gtest_main)
    
    # Register tests with CTest
    include(GoogleTest)
    gtest_discover_tests(ak_tests)
    
    # Custom test target
    add_custom_target(run_tests
        COMMAND ak_tests
        DEPENDS ak_tests
        COMMENT "Running tests..."
    )
endif()

# ============================================================================
# Coverage Configuration (Linux/macOS only)
# ============================================================================

if(NOT WIN32)
    find_program(GCOV_EXECUTABLE gcov)
    find_program(LCOV_EXECUTABLE lcov)
    find_program(GENHTML_EXECUTABLE genhtml)
    
    if(GCOV_EXECUTABLE AND LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory coverage_html
            COMMAND ${CMAKE_COMMAND} -E remove -f coverage.info coverage_base.info coverage_test.info
            
            # Clean previous coverage data
            COMMAND ${LCOV_EXECUTABLE} --directory . --zerocounters
            
            # Run tests
            COMMAND ${CMAKE_COMMAND} --build . --target ak_tests
            COMMAND ./ak_tests
            
            # Capture coverage data
            COMMAND ${LCOV_EXECUTABLE} --directory . --capture --output-file coverage.info
            
            # Filter out system headers and test files
            COMMAND ${LCOV_EXECUTABLE} --remove coverage.info '/usr/*' --output-file coverage.info
            COMMAND ${LCOV_EXECUTABLE} --remove coverage.info '*/tests/*' --output-file coverage.info
            COMMAND ${LCOV_EXECUTABLE} --remove coverage.info '*/googletest/*' --output-file coverage.info
            
            # Generate HTML report
            COMMAND ${GENHTML_EXECUTABLE} coverage.info --output-directory coverage_html
            COMMAND ${CMAKE_COMMAND} -E echo "📊 Coverage report generated in coverage_html/index.html"
            
            DEPENDS ak_tests
            COMMENT "🧪 Generating coverage report..."
            VERBATIM
        )
    endif()
endif()

# ============================================================================
# Installation Configuration
# ============================================================================

# Installation paths
if(WIN32)
    set(INSTALL_BIN_DIR "bin")
    set(INSTALL_COMPLETION_DIR "share/ak")
else()
    include(GNUInstallDirs)
    set(INSTALL_BIN_DIR ${CMAKE_INSTALL_BINDIR})
    if(APPLE)
        set(INSTALL_COMPLETION_DIR "share/ak")
    else()
        set(INSTALL_COMPLETION_DIR ${CMAKE_INSTALL_DATAROOTDIR}/ak)
    endif()
endif()

# Install binary
install(TARGETS ak
    RUNTIME DESTINATION ${INSTALL_BIN_DIR}
)

# Create shell completion files (Unix-like systems)
if(NOT WIN32)
    install(CODE "
        execute_process(
            COMMAND \${CMAKE_INSTALL_PREFIX}/${INSTALL_BIN_DIR}/ak completion bash
            OUTPUT_FILE bash_completion.tmp
        )
        execute_process(
            COMMAND \${CMAKE_INSTALL_PREFIX}/${INSTALL_BIN_DIR}/ak completion zsh  
            OUTPUT_FILE zsh_completion.tmp
        )
        execute_process(
            COMMAND \${CMAKE_INSTALL_PREFIX}/${INSTALL_BIN_DIR}/ak completion fish
            OUTPUT_FILE fish_completion.tmp
        )
    ")
    
    if(APPLE)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bash_completion.tmp
            DESTINATION /usr/local/etc/bash_completion.d
            RENAME ak
            OPTIONAL
        )
    else()
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bash_completion.tmp
            DESTINATION /etc/bash_completion.d
            RENAME ak
            OPTIONAL
        )
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/zsh_completion.tmp
            DESTINATION /usr/share/zsh/site-functions
            RENAME _ak
            OPTIONAL
        )
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fish_completion.tmp
            DESTINATION /usr/share/fish/completions
            RENAME ak.fish
            OPTIONAL
        )
    endif()
endif()

# Install man page (Unix-like systems only)
if(NOT WIN32)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/docs/ak.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
        OPTIONAL
    )
endif()

# Install desktop file and icons (Linux only)
if(UNIX AND NOT APPLE AND BUILD_GUI)
    # Install desktop file
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ak.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )
    
    # Install SVG icon
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/logo.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
        RENAME ak.svg
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )
    
    # Install PNG icon
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/logo.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
        RENAME ak.png
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )
    
    # Install Slint assets
    install(DIRECTORY ${SLINT_ASSETS_DIR}/
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/ak/slint-assets
        PATTERN "*" PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
    )
endif()

# For macOS, include Slint assets in the app bundle
if(APPLE AND BUILD_GUI)
    install(DIRECTORY ${SLINT_ASSETS_DIR}/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/Applications/AK.app/Contents/Resources/slint-assets
        PATTERN "*" PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
    )
endif()

# For Windows, install Slint assets alongside the executable
if(WIN32 AND BUILD_GUI)
    install(DIRECTORY ${SLINT_ASSETS_DIR}/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/slint-assets
        PATTERN "*" PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
    )
endif()

# ============================================================================
# Version Management and Publishing
# ============================================================================


# Version bump targets
add_custom_target(bump-patch
    COMMAND ${CMAKE_COMMAND} 
        -DPROJECT_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DBUMP_TYPE=patch
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version_bump.cmake
    COMMENT "🔄 Bumping patch version..."
)

add_custom_target(bump-minor
    COMMAND ${CMAKE_COMMAND}
        -DPROJECT_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DBUMP_TYPE=minor
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version_bump.cmake
    COMMENT "🔄 Bumping minor version..."
)

add_custom_target(bump-major
    COMMAND ${CMAKE_COMMAND}
        -DPROJECT_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DBUMP_TYPE=major
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version_bump.cmake
    COMMENT "🔄 Bumping major version..."
)

# Build release target
add_custom_target(build-release
    COMMAND ${CMAKE_COMMAND} --build . --config Release --target ak
    COMMENT "🏗️  Building release version..."
)

# Test release target
add_custom_target(test-release
    COMMAND ${CMAKE_COMMAND} -E echo "🧪 Running tests before release..."
    COMMAND ${CMAKE_COMMAND} --build . --config Release --target ak_tests
    COMMAND ./ak_tests
    COMMAND ${CMAKE_COMMAND} -E echo "✅ All tests passed"
    DEPENDS ak_tests
    COMMENT "🧪 Running release tests..."
)

# APT repository update target
add_custom_target(update-apt-repo
    COMMAND ${CMAKE_COMMAND}
        -DPROJECT_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apt_publish.cmake
    COMMENT "📦 Updating APT repository..."
)

# Publish targets
add_custom_target(publish
    COMMAND ${CMAKE_COMMAND} --build . --target bump-patch
    COMMAND ${CMAKE_COMMAND} --build . --target build-release
    COMMAND ${CMAKE_COMMAND} --build . --target update-apt-repo
    COMMAND ${CMAKE_COMMAND} --build . --target commit-and-push
    COMMENT "🚀 Publishing patch release..."
)

add_custom_target(publish-minor
    COMMAND ${CMAKE_COMMAND} --build . --target bump-minor
    COMMAND ${CMAKE_COMMAND} --build . --target build-release
    COMMAND ${CMAKE_COMMAND} --build . --target update-apt-repo
    COMMAND ${CMAKE_COMMAND} --build . --target commit-and-push
    COMMENT "🚀 Publishing minor release..."
)

add_custom_target(publish-major
    COMMAND ${CMAKE_COMMAND} --build . --target bump-major
    COMMAND ${CMAKE_COMMAND} --build . --target build-release
    COMMAND ${CMAKE_COMMAND} --build . --target update-apt-repo
    COMMAND ${CMAKE_COMMAND} --build . --target commit-and-push
    COMMENT "🚀 Publishing major release..."
)

# Git commit and push target
add_custom_target(commit-and-push
    COMMAND ${CMAKE_COMMAND}
        -DPROJECT_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/git_publish.cmake
    COMMENT "📦 Committing and pushing release..."
)

# ============================================================================
# Status Information
# ============================================================================

message(STATUS "")
message(STATUS "========================================")
message(STATUS "  AK (API Key Manager) v${AK_VERSION}")
message(STATUS "========================================")
message(STATUS "Platform: ${PLATFORM_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
if(BUILD_TESTS)
    message(STATUS "Tests: Enabled")
else()
    message(STATUS "Tests: Disabled")
endif()
message(STATUS "========================================")
message(STATUS "")
# CMake script for git commit and push
# Usage: cmake -DPROJECT_DIR=<dir> -P git_publish.cmake

if(NOT PROJECT_DIR)
    message(FATAL_ERROR "PROJECT_DIR must be specified")
endif()

# Read current version from CMakeLists.txt
file(READ "${PROJECT_DIR}/CMakeLists.txt" CMAKE_CONTENT)
string(REGEX MATCH "set\\(AK_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)\"" VERSION_MATCH ${CMAKE_CONTENT})

if(NOT VERSION_MATCH)
    message(FATAL_ERROR "Could not find version in CMakeLists.txt")
endif()

set(NEW_VERSION ${CMAKE_MATCH_1})

message(STATUS "📦 Committing and pushing release...")

# Check if there are changes to commit
execute_process(
    COMMAND git diff --quiet
    WORKING_DIRECTORY ${PROJECT_DIR}
    RESULT_VARIABLE GIT_DIFF_RESULT
)

if(GIT_DIFF_RESULT EQUAL 0)
    # Check for staged changes
    execute_process(
        COMMAND git diff --cached --quiet
        WORKING_DIRECTORY ${PROJECT_DIR}
        RESULT_VARIABLE GIT_STAGED_RESULT
    )
    
    if(GIT_STAGED_RESULT EQUAL 0)
        message(STATUS "⚠️  No changes to commit")
        return()
    endif()
endif()

# Add changed files
execute_process(
    COMMAND git add CMakeLists.txt src/core/config.cpp
    WORKING_DIRECTORY ${PROJECT_DIR}
    RESULT_VARIABLE ADD_RESULT
)

if(NOT ADD_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to add files to git")
endif()

# Commit changes
execute_process(
    COMMAND git commit -m "🚀 Release v${NEW_VERSION}"
    WORKING_DIRECTORY ${PROJECT_DIR}
    RESULT_VARIABLE COMMIT_RESULT
)

if(NOT COMMIT_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to commit changes")
endif()

# Create tag
execute_process(
    COMMAND git tag "v${NEW_VERSION}"
    WORKING_DIRECTORY ${PROJECT_DIR}
    RESULT_VARIABLE TAG_RESULT
)

if(NOT TAG_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to create git tag")
endif()

message(STATUS "🏷️  Created tag v${NEW_VERSION}")

# Check if remote origin exists
execute_process(
    COMMAND git remote get-url origin
    WORKING_DIRECTORY ${PROJECT_DIR}
    RESULT_VARIABLE REMOTE_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
)

if(REMOTE_RESULT EQUAL 0)
    message(STATUS "📤 Pushing to main branch...")
    
    # Push to main
    execute_process(
        COMMAND git push origin main
        WORKING_DIRECTORY ${PROJECT_DIR}
        RESULT_VARIABLE PUSH_RESULT
    )
    
    if(NOT PUSH_RESULT EQUAL 0)
        message(WARNING "Failed to push to main branch")
    endif()
    
    # Push tag
    execute_process(
        COMMAND git push origin "v${NEW_VERSION}"
        WORKING_DIRECTORY ${PROJECT_DIR}
        RESULT_VARIABLE PUSH_TAG_RESULT
    )
    
    if(NOT PUSH_TAG_RESULT EQUAL 0)
        message(WARNING "Failed to push tag")
    else()
        message(STATUS "✅ Successfully published v${NEW_VERSION} to GitHub")
    endif()
else()
    message(STATUS "⚠️  No remote 'origin' found. Skipping push to GitHub.")
    message(STATUS "✅ Version bumped and committed locally as v${NEW_VERSION}")
endif()
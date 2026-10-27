# CMakeLists.txt — ECE 309 Project 3 (MiniHarness: tools and the tool registry)
#
#   cmake -S . -B build                        # uses YOUR P2 Conversation / SentinelScanner
#   cmake -S . -B build -DUSE_P2_REFERENCE=ON  # uses the instructor's P2 reference instead
#   cmake -S . -B build -DUSE_P2_REFERENCE=OFF # switch back to yours later
#   cmake --build build && (cd build && ctest --output-on-failure)

cmake_minimum_required(VERSION 3.16)
project(MiniHarness VERSION 3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_P2_REFERENCE "Build with the instructor's P2 Conversation/SentinelScanner" OFF)
option(MH_SANITIZE      "Build with AddressSanitizer + UndefinedBehaviorSanitizer"   ON)
option(MH_WERROR        "Treat warnings as errors (the grader turns this on)"        OFF)

add_compile_options(-Wall -Wextra -Wpedantic -g)
if(MH_WERROR)
  add_compile_options(-Werror)
endif()
if(MH_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# ---- P2 carry-over: your code, or the reference -----------------------------
if(USE_P2_REFERENCE)
  message(STATUS "P2 code: instructor reference (reference/p2)")
  set(P2_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/reference/p2/include)
  set(P2_SOURCES reference/p2/src/conversation.cpp reference/p2/src/sentinel_scanner.cpp)
else()
  message(STATUS "P2 code: your own (include/core + src/core)")
  foreach(f include/core/conversation.h include/core/sentinel_scanner.h
            src/core/conversation.cpp   src/core/sentinel_scanner.cpp)
    if(NOT EXISTS ${CMAKE_SOURCE_DIR}/${f})
      message(FATAL_ERROR
        "\n  Missing ${f}.\n"
        "  Copy your P2 conversation.h/.cpp and sentinel_scanner.h/.cpp into\n"
        "  include/core/ and src/core/ (do NOT copy message.h), or configure with\n"
        "      cmake -S . -B build -DUSE_P2_REFERENCE=ON\n"
        "  to build against the instructor's P2 reference.\n")
    endif()
  endforeach()
  set(P2_INCLUDE_DIR "")
  set(P2_SOURCES src/core/conversation.cpp src/core/sentinel_scanner.cpp)
endif()

# ---- Everything except main() lives in one library --------------------------
add_library(harness_core STATIC
  ${P2_SOURCES}
  # P3: you implement
  src/tools/tool_registry.cpp
  src/tools/workspace.cpp
  src/tools/calculator_eval.cpp
  # P3: provided
  src/json/json_value.cpp
  src/tools/workspace_provided.cpp
  src/tools/calculator_tokenize.cpp
  src/tools/calculator_tool.cpp
  src/tools/file_tools.cpp
  src/tools/builtin_tools.cpp
  src/harness/tool_dispatcher.cpp
  src/harness/harness.cpp
  src/harness/transcript.cpp
  src/model/model_client.cpp
  src/model/scripted_client.cpp
  src/model/replay_client.cpp
)
# The reference P2 headers (if selected) must be found BEFORE include/.
target_include_directories(harness_core PUBLIC ${P2_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/include)

add_executable(miniharness src/main.cpp)
target_link_libraries(miniharness PRIVATE harness_core)

# ---- Tests -------------------------------------------------------------------
enable_testing()

add_executable(test_p2 tests/p2/test_p2.cpp)
target_link_libraries(test_p2 PRIVATE harness_core)
add_test(NAME p2_regression COMMAND test_p2 WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(test_p3 tests/p3/test_p3.cpp)            # your tests
target_link_libraries(test_p3 PRIVATE harness_core)
add_test(NAME p3 COMMAND test_p3 WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(test_loop tests/p3/test_loop.cpp)         # provided tool-loop tests
target_link_libraries(test_loop PRIVATE harness_core)
add_test(NAME tool_loop COMMAND test_loop WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

if(EXISTS ${CMAKE_SOURCE_DIR}/tests/provided/test_provided.cpp)   # instructor only
  add_executable(test_provided tests/provided/test_provided.cpp)
  target_link_libraries(test_provided PRIVATE harness_core)
  add_test(NAME provided COMMAND test_provided WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/tests/e2e/run_e2e.sh)
  add_test(NAME e2e
           COMMAND bash ${CMAKE_SOURCE_DIR}/tests/e2e/run_e2e.sh $<TARGET_FILE:miniharness>
           WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

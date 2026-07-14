cmake_minimum_required(VERSION 3.16)
project(snmpmon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SNMPMON_BUILD_TESTS "Build unit tests" OFF)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra)
  add_compile_options(-static-libgcc -static-libstdc++)
  add_link_options(-static-libgcc -static-libstdc++)
endif()

# snmpmon_core sources are added incrementally phase by phase (see cpp/README.md).
add_library(snmpmon_core STATIC
  src/snmp/oid.cpp
  src/snmp/ber.cpp
  src/snmp/pdu.cpp
  src/snmp/udp_socket.cpp
  src/snmp/client.cpp
  src/poll/if_table.cpp
  src/poll/device_state.cpp
  src/poll/poller.cpp
  src/config/config.cpp
  src/http/json_writer.cpp
  src/http/routes_status.cpp
  src/http/web_assets.cpp
  src/http/routes_metrics.cpp
  src/http/server.cpp
)

target_include_directories(snmpmon_core PUBLIC src)
# SYSTEM: these are vendored headers we don't control; suppress their
# warnings under our own -Wall/-Wextra/-W4 rather than treating a
# third-party library's unused-parameter etc. as something to fix here.
target_include_directories(snmpmon_core SYSTEM PUBLIC
  third_party/httplib
  third_party/sha256
)

if(WIN32)
  target_link_libraries(snmpmon_core PUBLIC ws2_32)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(snmpmon_core PUBLIC Threads::Threads)
endif()

add_executable(snmpmon src/main.cpp)
target_link_libraries(snmpmon PRIVATE snmpmon_core)

if(SNMPMON_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

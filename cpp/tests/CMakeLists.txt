add_executable(snmpmon_tests
  test_main.cpp
  test_oid.cpp
  test_ber.cpp
  test_client_loopback.cpp
  test_config.cpp
  test_if_table.cpp
  test_walk_loopback.cpp
  test_device_state.cpp
  test_poller_loopback.cpp
  test_json_writer.cpp
  test_routes_status.cpp
  test_routes_metrics.cpp
  test_http_server.cpp
  test_auth.cpp
  test_routes_auth.cpp
)

target_include_directories(snmpmon_tests SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
target_link_libraries(snmpmon_tests PRIVATE snmpmon_core)

if(NOT WIN32)
  find_package(Threads REQUIRED)
  target_link_libraries(snmpmon_tests PRIVATE Threads::Threads)
endif()

include(CTest)
add_test(NAME snmpmon_tests COMMAND snmpmon_tests)

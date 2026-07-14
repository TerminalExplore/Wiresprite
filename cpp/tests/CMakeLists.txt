add_executable(wiresprite_tests
  test_main.cpp
  test_oid.cpp
  test_ber.cpp
  test_client_loopback.cpp
  test_config.cpp
  test_if_table.cpp
  test_walk_loopback.cpp
  test_device_state.cpp
  test_history_store.cpp
  test_sse_hub.cpp
  test_mac_table.cpp
  test_poller_loopback.cpp
  test_json_writer.cpp
  test_routes_status.cpp
  test_routes_metrics.cpp
  test_http_server.cpp
  test_auth.cpp
  test_routes_auth.cpp
)

target_include_directories(wiresprite_tests SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
target_link_libraries(wiresprite_tests PRIVATE wiresprite_core)

if(NOT WIN32)
  find_package(Threads REQUIRED)
  target_link_libraries(wiresprite_tests PRIVATE Threads::Threads)
endif()

include(CTest)
add_test(NAME wiresprite_tests COMMAND wiresprite_tests)

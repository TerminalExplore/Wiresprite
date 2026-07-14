add_executable(snmpmon_tests
  test_main.cpp
  test_oid.cpp
  test_ber.cpp
)

target_include_directories(snmpmon_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
target_link_libraries(snmpmon_tests PRIVATE snmpmon_core)

include(CTest)
add_test(NAME snmpmon_tests COMMAND snmpmon_tests)

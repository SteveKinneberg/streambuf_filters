add_executable(tabulator_test tabulator.cc)
target_link_libraries(tabulator_test streambuf-filters gtest gtest_main)
gtest_discover_tests(tabulator_test)
add_dependencies(${UNIT_TESTS_TARGET} tabulator_test)

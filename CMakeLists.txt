cmake_minimum_required(VERSION 3.26)

project(streambuf-filters
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "C++ stream buffer filters"
)

# Pull in local .cmake modules
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${PROJECT_SOURCE_DIR}/cmake")
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" is_top_level)
if(CMAKE_CROSSCOMPILING)
    set(build_testing_default "FALSE")
else()
    set(build_testing_default "${is_top_level}")
endif()

# Additional CMake options
option(BUILD_TESTING "Build test code and run tests as part of build" "${build_testing_default}")
option(BUILD_COVERAGE "Build code coverage" OFF)
option(${PROJECT_NAME}_INCLUDE_PACKAGING "Include packaging rules for ${PROJECT_NAME}" "${is_top_level}")
option(BUILD_DOCUMENTATION "Build HTML documentation" ON)

# Set C++ standard - do not use compiler extensions
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS False)
set(CMAKE_CXX_STANDARD 20)

include(DoxygenConfig)

if(BUILD_COVERAGE)
    include(CodeCoverage)
    append_coverage_compiler_flags()
    setup_target_for_coverage_gcovr_html(
        NAME ${PROJECT_NAME}_coverage
        EXECUTABLE ${UNIT_TEST_TARGETS}
        EXCLUDE examples test gtest
    )
endif()

if(BUILD_TESTING)
    include(CTest)
    include(GoogleTest)

    # Google Test/Mock does not provide appropriate scripts for `find_package()`.  Some Linux
    # distributions provide their own but they are inconsistent with each other and thus not usable.
    find_package(PkgConfig REQUIRED)

    # There is a problem with GCC 11.x and older when building GoogleTest 1.11 and newer when
    # GoogleTest tries to print a class/struct instance that provides an operator<< override for
    # output and that class/struct has a templated constructor.  To further complicate matters
    # GoogleTest 1.10 cannot be built by GCC 11.x and newer.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
            AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "11.0"
            AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "12.0")
        message(FATAL_ERROR
            "Cannot build GoogleTest with GCC/G++ 11.x\n"
            "  Either upgrade to 12.x+ or downgrade to 10.x")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
            AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "11.0")
        pkg_check_modules(gtest gtest=1.10.0 gmock=1.10.0)
        set(GET_GTEST_VERSION "1.10.0")
    else()
        pkg_check_modules(gtest gtest>=1.10.0 gmock>=1.10.0)
        set(GET_GTEST_VERSION "1.12.1")
    endif()

    if(NOT gtest_FOUND)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
                AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS_EQUAL "12.0")
            message(FATAL_ERROR "Cannot build Unit Tests")
        endif()

        message(STATUS "  Download and build gtest v${GET_GTEST_VERSION} directly")
        include(FetchContent)

        FetchContent_Declare(gtest
            SYSTEM
            GIT_REPOSITORY "https://github.com/google/googletest.git"
            GIT_TAG "release-${GET_GTEST_VERSION}")
        # For Windows: Prevent overriding the parent project's compiler/linker settings
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(gtest)

    endif()

    enable_testing()
    set(UNIT_TESTS_TARGET ${PROJECT_NAME}_unit_tests)
    add_custom_target(${UNIT_TESTS_TARGET} ALL)

    # Only run unit tests if not cross compiling
    if(NOT CMAKE_CROSSCOMPILING)
        add_custom_command(TARGET ${PROJECT_NAME}_unit_tests
            COMMENT "Run unit tests"
            POST_BUILD COMMAND ctest ARGS --progress --output-on-failure
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    endif()

    add_subdirectory(test)
endif()

add_library(streambuf-filters STATIC src/tabulator.cc src/logger.cc)
target_include_directories(streambuf-filters PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include;${CMAKE_BINARY_DIR}/include>"
    $<INSTALL_INTERFACE:include>
)

add_executable(logger_sample samples/logger_sample.cc)
target_link_libraries(logger_sample PRIVATE streambuf-filters)
add_executable(tabulator_sample samples/tabulator_sample.cc)
target_link_libraries(tabulator_sample PRIVATE streambuf-filters)



if (BUILD_DOCUMENTATION)
    document(README.md)
    document(INTERNAL docs/internal)
    doxygen_public(${PROJECT_NAME}_public_docs
        "C++ stream buffer filters"
        "Utilties for modifying output using C++ IO streams"
    )
    doxygen_internal(${PROJECT_NAME}_internal_docs
        "C++ stream buffer filters"
        "Utilties for modifying output using C++ IO streams developer documentation"
    )
endif()


if (${PROJECT_NAME}_INCLUDE_PACKAGING)
    add_subdirectory(packaging)
endif()

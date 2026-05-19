# =============================================================================
# Tests.  We use system GoogleTest (libgtest-dev) on Linux, falling back to
# FetchContent if it isn't installed.  This means CI-style environments and
# developer-laptop environments both work without manual fiddling.
# =============================================================================

find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message(STATUS "GoogleTest not found; fetching v1.14.0")
    include(FetchContent)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.17.0)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    add_library(GTest::gtest      ALIAS gtest)
    add_library(GTest::gtest_main ALIAS gtest_main)
endif()

include(GoogleTest)

# -----------------------------------------------------------------------------
# Helper: register a single-source test binary linked against `astara` and
# GoogleTest, with all the tests autoregistered via gtest_discover_tests.
# -----------------------------------------------------------------------------
function(astara_add_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name}
            PRIVATE astara
                    astara_warnings
                    GTest::gtest
                    GTest::gtest_main)
    gtest_discover_tests(${name})
endfunction()

# Unit tests: utilities, plumbing, single-class behaviour.
astara_add_test(test_state         unit/test_state.cpp)
astara_add_test(test_integrator    unit/test_integrator.cpp)
astara_add_test(test_units         unit/test_units.cpp)
astara_add_test(test_water         unit/test_water.cpp)
astara_add_test(test_controllers   unit/test_controllers.cpp)

# Analytical tests: closed-form solutions to physics problems, used to earn
# trust in each component before integrating them in a primary loop.
astara_add_test(test_kinetics_analytical    analytical/test_kinetics_analytical.cpp)
astara_add_test(test_reactor_analytical     analytical/test_reactor_analytical.cpp)
astara_add_test(test_pump_analytical        analytical/test_pump_analytical.cpp)
astara_add_test(test_pressurizer_analytical analytical/test_pressurizer_analytical.cpp)
astara_add_test(test_sg_analytical          analytical/test_sg_analytical.cpp)
astara_add_test(test_helical_sg_analytical  analytical/test_helical_sg_analytical.cpp)

# Integration tests: full-loop dynamics.
astara_add_test(test_primary_loop           integration/test_primary_loop.cpp)

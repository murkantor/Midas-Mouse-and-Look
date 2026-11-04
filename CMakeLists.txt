cmake_minimum_required(VERSION 3.13)
project(midas_mouse_and_look C)

# midas_feel_core : stick shaping, look curve, mouse - pure C99, no dependencies.
#
# As a subproject:   add_subdirectory(third_party/midas-mouse-and-look)
#                    target_link_libraries(game PRIVATE midas_feel_core)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(MIDAS_FEEL_TOP ON)
else()
    set(MIDAS_FEEL_TOP OFF)
endif()

option(MIDAS_FEEL_TESTS "Build the tests" ${MIDAS_FEEL_TOP})

add_library(midas_feel_core STATIC
    src/mf_stick.c
    src/mf_look.c
    src/mf_mouse.c)
target_include_directories(midas_feel_core PUBLIC include)
set_property(TARGET midas_feel_core PROPERTY C_STANDARD 99)
if(NOT MSVC)
    target_link_libraries(midas_feel_core PUBLIC m)
endif()

if(MIDAS_FEEL_TESTS)
    enable_testing()
    add_executable(mf_test tests/mf_test.c)
    target_link_libraries(mf_test PRIVATE midas_feel_core)
    add_test(NAME mf_test COMMAND mf_test)
endif()

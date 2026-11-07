# (c) Glape Contributors

cmake_minimum_required(VERSION 3.16)
project(glape LANGUAGES C)

set(GLAPE_VERSION "1.0" CACHE STRING "Version shown by glape version")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

file(GLOB LIB_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/libs/*.c")

add_executable(glape main.c cli/commands.c cli/console.c cli/dispatcher.c
                       mem/gc.c mem/util.c mem/value.c parse/ast.c parse/lexer.c parse/parser.c
                       runtime/env.c runtime/error.c runtime/interp.c ${LIB_SOURCES})
target_include_directories(glape PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(glape PRIVATE GLAPE_VERSION="${GLAPE_VERSION}")

if(MSVC)
    target_compile_options(glape PRIVATE /W4)
else()
    target_compile_options(glape PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(glape PRIVATE m)
endif()

install(TARGETS glape RUNTIME DESTINATION bin)

include(CTest)

if(BUILD_TESTING)
    file(GLOB LEXER_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.glape")

    foreach(source ${LEXER_TESTS})
        get_filename_component(test_name "${source}" NAME_WE)
        get_filename_component(test_directory "${source}" DIRECTORY)

        add_test(
            NAME lexer_${test_name}
            COMMAND "${CMAKE_COMMAND}"
                "-DEXE=$<TARGET_FILE:glape>"
                "-DINPUT=${source}"
                "-DEXPECTED=${test_directory}/${test_name}.expected"
                "-DACTUAL=${CMAKE_CURRENT_BINARY_DIR}/${test_name}.actual"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_lexer_test.cmake"
        )
    endforeach()

    file(GLOB AST_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/*.glape")

    foreach(source ${AST_TESTS})
        get_filename_component(test_name "${source}" NAME_WE)
        get_filename_component(test_directory "${source}" DIRECTORY)

        add_test(
            NAME ast_${test_name}
            COMMAND "${CMAKE_COMMAND}"
                "-DEXE=$<TARGET_FILE:glape>"
                "-DINPUT=${source}"
                "-DEXPECTED=${test_directory}/${test_name}.expected"
                "-DACTUAL=${CMAKE_CURRENT_BINARY_DIR}/ast_${test_name}.actual"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_ast_test.cmake"
        )
    endforeach()

    file(GLOB RUN_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/run/*.glape")

    foreach(source ${RUN_TESTS})
        get_filename_component(test_name "${source}" NAME_WE)
        get_filename_component(test_directory "${source}" DIRECTORY)

        add_test(
            NAME run_${test_name}
            COMMAND "${CMAKE_COMMAND}"
                "-DEXE=$<TARGET_FILE:glape>"
                "-DINPUT=${source}"
                "-DEXPECTED=${test_directory}/${test_name}.expected"
                "-DACTUAL=${CMAKE_CURRENT_BINARY_DIR}/run_${test_name}.actual"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_script_test.cmake"
        )

        add_test(
            NAME stress_${test_name}
            COMMAND "${CMAKE_COMMAND}"
                "-DEXE=$<TARGET_FILE:glape>"
                "-DINPUT=${source}"
                "-DEXPECTED=${test_directory}/${test_name}.expected"
                "-DACTUAL=${CMAKE_CURRENT_BINARY_DIR}/stress_${test_name}.actual"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_script_test.cmake"
        )
        set_tests_properties(stress_${test_name} PROPERTIES ENVIRONMENT "GLAPE_GC_STRESS=1")
    endforeach()

    add_test(
        NAME gc_memory
        COMMAND "${CMAKE_COMMAND}"
            "-DEXE=$<TARGET_FILE:glape>"
            "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/run/gc_memory.glape"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/tests/run/gc_memory.expected"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/gc_memory_test.cmake"
    )

    file(GLOB ERROR_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/errors/*.glape")

    foreach(source ${ERROR_TESTS})
        get_filename_component(test_name "${source}" NAME_WE)
        get_filename_component(test_directory "${source}" DIRECTORY)

        add_test(
            NAME error_${test_name}
            COMMAND "${CMAKE_COMMAND}"
                "-DEXE=$<TARGET_FILE:glape>"
                "-DINPUT=${source}"
                "-DEXPECTED=${test_directory}/${test_name}.expected"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_error_test.cmake"
        )
    endforeach()
endif()

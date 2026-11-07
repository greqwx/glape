# (c) Glape Contributors

execute_process(
    COMMAND "${EXE}" run "${INPUT}"
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    RESULT_VARIABLE result
)

if(result EQUAL 0)
    message(FATAL_ERROR "expected failure for ${INPUT}, but it succeeded\noutput:\n${output}")
endif()

file(READ "${EXPECTED}" expected)

string(REPLACE "${INPUT}" "<file>" errors "${errors}")
string(REPLACE "\r\n" "\n" errors "${errors}")
string(REPLACE "\r\n" "\n" expected "${expected}")
string(REGEX REPLACE "\n+$" "" errors "${errors}")
string(REGEX REPLACE "\n+$" "" expected "${expected}")

if(NOT errors STREQUAL expected)
    message(FATAL_ERROR "error output mismatch for ${INPUT}\n--- expected ---\n${expected}\n--- actual ---\n${errors}")
endif()

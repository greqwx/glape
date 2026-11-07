# (c) Glape Contributors

execute_process(
    COMMAND "${EXE}" ast "${INPUT}"
    OUTPUT_FILE "${ACTUAL}"
    ERROR_VARIABLE errors
    RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "glape failed on ${INPUT}: ${errors}")
endif()

file(READ "${ACTUAL}" actual)
file(REMOVE "${ACTUAL}")
file(READ "${EXPECTED}" expected)

string(REPLACE "\r\n" "\n" actual "${actual}")
string(REPLACE "\r\n" "\n" expected "${expected}")
string(REGEX REPLACE "\n+$" "" actual "${actual}")
string(REGEX REPLACE "\n+$" "" expected "${expected}")

if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "ast dump mismatch for ${INPUT}\n--- expected ---\n${expected}\n--- actual ---\n${actual}")
endif()

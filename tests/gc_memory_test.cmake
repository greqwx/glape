# (c) Glape Contributors

if(NOT DEFINED MIN_COLLECTIONS)
    set(MIN_COLLECTIONS 2)
endif()

if(NOT DEFINED MAX_LIVE_BYTES)
    set(MAX_LIVE_BYTES 16384)
endif()

if(NOT DEFINED MAX_LIVE_OBJECTS)
    set(MAX_LIVE_OBJECTS 64)
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "GLAPE_GC_STATS=1" "${EXE}" run "${INPUT}"
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "glape failed on ${INPUT}: ${errors}")
endif()

file(READ "${EXPECTED}" expected)

string(REPLACE "\r\n" "\n" output "${output}")
string(REPLACE "\r\n" "\n" expected "${expected}")
string(REGEX REPLACE "\n+$" "" output "${output}")
string(REGEX REPLACE "\n+$" "" expected "${expected}")

if(NOT output STREQUAL expected)
    message(FATAL_ERROR "output mismatch for ${INPUT}\n--- expected ---\n${expected}\n--- actual ---\n${output}")
endif()

if(NOT errors MATCHES "gc: collections=([0-9]+) live_objects=([0-9]+) live_bytes=([0-9]+)")
    message(FATAL_ERROR "missing gc stats for ${INPUT}: ${errors}")
endif()

set(collections "${CMAKE_MATCH_1}")
set(live_objects "${CMAKE_MATCH_2}")
set(live_bytes "${CMAKE_MATCH_3}")

if(collections LESS MIN_COLLECTIONS)
    message(FATAL_ERROR "expected at least ${MIN_COLLECTIONS} collections, got ${collections}")
endif()

if(live_objects GREATER MAX_LIVE_OBJECTS)
    message(FATAL_ERROR "too many live objects: ${live_objects} > ${MAX_LIVE_OBJECTS}")
endif()

if(live_bytes GREATER MAX_LIVE_BYTES)
    message(FATAL_ERROR "live bytes not reclaimed: ${live_bytes} > ${MAX_LIVE_BYTES}")
endif()

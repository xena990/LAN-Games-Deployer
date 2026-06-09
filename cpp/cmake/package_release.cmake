if(NOT DEFINED INPUT_DIR OR NOT DEFINED OUTPUT_DIR)
  message(FATAL_ERROR "INPUT_DIR and OUTPUT_DIR are required")
endif()

file(TO_CMAKE_PATH "${INPUT_DIR}" INPUT_DIR_N)
file(TO_CMAKE_PATH "${OUTPUT_DIR}" OUTPUT_DIR_N)

set(IN_EXE "${INPUT_DIR_N}/LANGamesDeployerCpp.exe")
if(NOT EXISTS "${IN_EXE}")
  message(FATAL_ERROR "Built executable not found: ${IN_EXE}")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR_N}")
file(COPY "${IN_EXE}" DESTINATION "${OUTPUT_DIR_N}")

if(DEFINED OUTPUT_EXE_NAME AND NOT OUTPUT_EXE_NAME STREQUAL "")
  file(RENAME "${OUTPUT_DIR_N}/LANGamesDeployerCpp.exe" "${OUTPUT_DIR_N}/${OUTPUT_EXE_NAME}")
endif()

set(IN_DATA "${INPUT_DIR_N}/data")
if(EXISTS "${IN_DATA}")
  file(MAKE_DIRECTORY "${OUTPUT_DIR_N}/data")
  file(COPY "${IN_DATA}/" DESTINATION "${OUTPUT_DIR_N}/data")
endif()

message(STATUS "Release bundle ready: ${OUTPUT_DIR_N}")

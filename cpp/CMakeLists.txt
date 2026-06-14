cmake_minimum_required(VERSION 3.16)
project(LANGamesDeployerCpp VERSION 1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/W4 /EHsc /DUNICODE /D_UNICODE)
  # Static runtime: no VC++ redistributable needed on target machines.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  # For old Windows targets (including XP) use the VS v141_xp toolset when configuring:
  # cmake -T v141_xp ...
endif()

add_executable(LANGamesDeployerCpp WIN32
  src/main.cpp
  src/app.rc
)

target_link_libraries(LANGamesDeployerCpp PRIVATE
  comctl32
  ws2_32
  user32
  gdi32
  shell32
)

set(LGD_RELEASE_DIR "${CMAKE_SOURCE_DIR}/release/LAN Games Deployer v${PROJECT_VERSION}")

add_custom_target(release_bundle
  COMMAND ${CMAKE_COMMAND}
    -DINPUT_DIR=$<TARGET_FILE_DIR:LANGamesDeployerCpp>
    -DOUTPUT_DIR=${LGD_RELEASE_DIR}
    -DDATA_DIR=${CMAKE_SOURCE_DIR}/../data
    -DOUTPUT_EXE_NAME=LANGamesDeployer.exe
    -P ${CMAKE_SOURCE_DIR}/cmake/package_release.cmake
  DEPENDS LANGamesDeployerCpp
  COMMENT "Building release bundle in: ${LGD_RELEASE_DIR}"
)

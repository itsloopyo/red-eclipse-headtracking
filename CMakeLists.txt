cmake_minimum_required(VERSION 3.20)
project(RedEclipseHeadTracking VERSION 0.3.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "RedEclipseHeadTracking must be built as x64. Configure with -A x64.")
endif()

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CAMERAUNLOCK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(cameraunlock-core/cpp ${CMAKE_BINARY_DIR}/cameraunlock-core)

set(MINHOOK_SOURCES
    extern/minhook/src/buffer.c
    extern/minhook/src/hook.c
    extern/minhook/src/trampoline.c
    extern/minhook/src/hde/hde32.c
    extern/minhook/src/hde/hde64.c
)

set(MOD_SOURCES
    src/dllmain.cpp
    src/config.cpp
    src/legacy_config/legacy_config.cpp
    src/path_utils.cpp
    src/hotkeys.cpp
    src/tracking_runtime.cpp
    src/game_symbols.cpp
    src/camera_hook.cpp
)

add_library(RedEclipseHeadTracking SHARED
    ${MINHOOK_SOURCES}
    ${MOD_SOURCES}
)

set_target_properties(RedEclipseHeadTracking PROPERTIES
    OUTPUT_NAME "RedEclipseHeadTracking"
    SUFFIX ".asi"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

target_include_directories(RedEclipseHeadTracking PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/minhook/include
)

target_link_libraries(RedEclipseHeadTracking PRIVATE cameraunlock)

if(MSVC)
    target_compile_options(RedEclipseHeadTracking PRIVATE /W4 /WX /permissive- /wd4324)
    set_source_files_properties(${MINHOOK_SOURCES} PROPERTIES COMPILE_FLAGS "/W0")
endif()

if(WIN32)
    # dbghelp resolves the game's symbols by name from the PDB it ships with.
    target_link_libraries(RedEclipseHeadTracking PRIVATE ws2_32 dbghelp)
endif()

# ---------- host-side unit tests (no engine dependency) ----------
option(HEADTRACKING_BUILD_TESTS "Build the host-side unit tests" OFF)

if(HEADTRACKING_BUILD_TESTS)
    add_executable(head_transform_test test/head_transform_test.cpp)
    target_include_directories(head_transform_test PRIVATE src)
    if(MSVC)
        target_compile_options(head_transform_test PRIVATE /W4 /WX /permissive-)
    endif()
    enable_testing()
    add_test(NAME head_transform_test COMMAND head_transform_test)
    add_executable(ads_test test/ads_test.cpp)
    target_include_directories(ads_test PRIVATE src cameraunlock-core/cpp/include)
    if(MSVC)
        target_compile_options(ads_test PRIVATE /W4 /WX /permissive-)
    endif()
    add_test(NAME ads_test COMMAND ads_test)

    # The config differential test. The oracle is v0.3.1's reader, the newest published
    # build, copied byte for byte into tests/config_differential/oracle with the one core
    # header it included that core has since removed; renaming its namespace lets it link
    # beside this build's. oracle_adapter.cpp copies its Config out for the test.
    set(RE_ORACLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/config_differential/oracle)
    add_library(re_config_oracle STATIC
        ${RE_ORACLE_DIR}/src/config.cpp
        tests/config_differential/oracle_adapter.cpp
    )
    target_include_directories(re_config_oracle PRIVATE
        ${RE_ORACLE_DIR}/src
        ${RE_ORACLE_DIR}/core/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/config_differential)
    target_compile_definitions(re_config_oracle PRIVATE RedEclipseHeadTracking=re_oracle)
    target_link_libraries(re_config_oracle PRIVATE cameraunlock)

    add_executable(config_differential_test
        tests/config_differential/differential_tests.cpp
        src/legacy_config/legacy_config.cpp
    )
    target_include_directories(config_differential_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/config_differential)
    target_compile_definitions(config_differential_test PRIVATE
        RE_DIFFERENTIAL_INPUTS="${CMAKE_CURRENT_SOURCE_DIR}/tests/config_differential/inputs")
    target_link_libraries(config_differential_test PRIVATE cameraunlock re_config_oracle)
    if(MSVC)
        target_compile_options(config_differential_test PRIVATE /W4 /WX /permissive-)
    endif()
    add_test(NAME config_differential COMMAND config_differential_test)

    # The frozen reader, the core sources it compiles, the oracle and the first-run inputs
    # against the hashes tests/config_differential/frozen.tsv records.
    add_test(NAME config_frozen COMMAND powershell -NoProfile -ExecutionPolicy Bypass
        -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/config_differential/check-frozen.ps1)
endif()

cmake_minimum_required(VERSION 3.20)
project(RedEclipseHeadTracking VERSION 0.2.0 LANGUAGES C CXX)

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
endif()

cmake_minimum_required(VERSION 3.22.1)
project(switch_runtime LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RUNTIME_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../")
set(COMPAT_ROOT "${RUNTIME_ROOT}/runtime/compat")
set(COMPAT_DARWIN "${COMPAT_ROOT}/darwin")
set(TLS_ROOT "${RUNTIME_ROOT}/runtime/tls")

set(COMPAT_SOURCES
    "${COMPAT_ROOT}/dm_compat_audio.c"
    "${COMPAT_ROOT}/dm_compat_blocks.c"
    "${COMPAT_ROOT}/dm_compat_fcntl.c"
    "${COMPAT_ROOT}/dm_compat_macho.c"
    "${COMPAT_ROOT}/dm_compat_macho_exec.c"
    "${COMPAT_ROOT}/dm_compat_md5.c"
    "${COMPAT_ROOT}/dm_compat_sha256.c"
    "${COMPAT_ROOT}/dm_compat_selftest.c"
    "${COMPAT_ROOT}/dm_compat_xattr.c"
    "${TLS_ROOT}/android_tls.c"
    "${TLS_ROOT}/android_tls_thunk.c"
)

add_library(switch_runtime SHARED
    runtime.cpp
    ${COMPAT_SOURCES}
    "${TLS_ROOT}/tls_trampoline.S"
)

target_include_directories(switch_runtime PRIVATE
    "${COMPAT_DARWIN}"
    "${COMPAT_ROOT}"
    "${TLS_ROOT}"
)

target_compile_options(switch_runtime PRIVATE
    -fblocks
    -fno-stack-protector
    -Wall
    -Wextra
    -Wno-unused-parameter
)

target_link_libraries(switch_runtime
    android
    log
    aaudio
    vulkan
)

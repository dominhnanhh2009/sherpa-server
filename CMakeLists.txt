cmake_minimum_required(VERSION 3.20)
project(sherpa-server LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Drogon package
find_package(Drogon REQUIRED)

# Sherpa-onnx SDK path
set(SHERPA_ONNX_ROOT "C:/sherpaonnx" CACHE PATH "Path to sherpa-onnx prebuilt SDK")

if(NOT EXISTS "${SHERPA_ONNX_ROOT}/include/sherpa-onnx/c-api/c-api.h")
    message(FATAL_ERROR "sherpa-onnx C API header not found at ${SHERPA_ONNX_ROOT}")
endif()

file(GLOB_RECURSE SRC_FILES
    "src/*.cpp"
)

# Executable target
add_executable(sherpa-server
    ${SRC_FILES}
)

target_include_directories(sherpa-server PRIVATE
    "${SHERPA_ONNX_ROOT}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(sherpa-server
    PRIVATE
        Drogon::Drogon
        "${SHERPA_ONNX_ROOT}/lib/sherpa-onnx-c-api.lib"
)

if(WIN32)
    target_link_libraries(sherpa-server
        PRIVATE
            ws2_32
            crypt32
            secur32
            rpcrt4
            shlwapi
    )

    # Copy required DLLs to target binary directory
    add_custom_command(TARGET sherpa-server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SHERPA_ONNX_ROOT}/lib/sherpa-onnx-c-api.dll"
            "${SHERPA_ONNX_ROOT}/lib/onnxruntime.dll"
            "${SHERPA_ONNX_ROOT}/lib/onnxruntime_providers_shared.dll"
            $<TARGET_FILE_DIR:sherpa-server>
    )
endif()

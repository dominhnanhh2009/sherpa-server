cmake_minimum_required(VERSION 3.20)
project(sherpa-server LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Workaround for Drogon's FindFilesystem try_run under Windows MinGW/UCRT
set(CXX_FILESYSTEM_NO_LINK_NEEDED TRUE CACHE BOOL "Filesystem no link needed")
set(Filesystem_FOUND TRUE CACHE BOOL "Filesystem found")

# Find Drogon package
find_package(Drogon REQUIRED)

# Sherpa-onnx prebuilt SDK
set(SHERPA_ONNX_ROOT "" CACHE PATH "Path to sherpa-onnx prebuilt SDK (optional)")

if(SHERPA_ONNX_ROOT AND EXISTS "${SHERPA_ONNX_ROOT}/include/sherpa-onnx/c-api/c-api.h")
    message(STATUS "Using user-specified sherpa-onnx SDK: ${SHERPA_ONNX_ROOT}")
else()
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()
    include(FetchContent)
    message(STATUS "Auto-fetching sherpa-onnx prebuilt SDK (v1.13.8)...")
    FetchContent_Declare(
        sherpa_onnx_prebuilt
        URL "https://github.com/k2-fsa/sherpa-onnx/releases/download/v1.13.8/sherpa-onnx-v1.13.8-win-x64-shared-MT-Release.tar.bz2"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_GetProperties(sherpa_onnx_prebuilt)
    if(NOT sherpa_onnx_prebuilt_POPULATED)
        FetchContent_Populate(sherpa_onnx_prebuilt)
    endif()

    if(EXISTS "${sherpa_onnx_prebuilt_SOURCE_DIR}/include/sherpa-onnx/c-api/c-api.h")
        set(SHERPA_ONNX_ROOT "${sherpa_onnx_prebuilt_SOURCE_DIR}")
    elseif(EXISTS "${sherpa_onnx_prebuilt_SOURCE_DIR}/sherpa-onnx-v1.13.8-win-x64-shared-MT-Release/include/sherpa-onnx/c-api/c-api.h")
        set(SHERPA_ONNX_ROOT "${sherpa_onnx_prebuilt_SOURCE_DIR}/sherpa-onnx-v1.13.8-win-x64-shared-MT-Release")
    else()
        file(GLOB_RECURSE C_API_HEADER "${sherpa_onnx_prebuilt_SOURCE_DIR}/*c-api.h")
        if(C_API_HEADER)
            list(GET C_API_HEADER 0 FIRST_HEADER)
            get_filename_component(C_API_DIR "${FIRST_HEADER}" DIRECTORY)
            get_filename_component(SHERPA_ONNX_ROOT "${C_API_DIR}/../../.." ABSOLUTE)
        else()
            message(FATAL_ERROR "Could not locate c-api.h in fetched sherpa-onnx directory: ${sherpa_onnx_prebuilt_SOURCE_DIR}")
        endif()
    endif()
endif()

message(STATUS "SHERPA_ONNX_ROOT: ${SHERPA_ONNX_ROOT}")

# Locate libraries and DLLs
find_library(SHERPA_ONNX_LIB
    NAMES sherpa-onnx-c-api
    PATHS "${SHERPA_ONNX_ROOT}/lib"
    NO_DEFAULT_PATH
    REQUIRED
)

find_file(SHERPA_ONNX_DLL
    NAMES sherpa-onnx-c-api.dll
    PATHS "${SHERPA_ONNX_ROOT}/lib" "${SHERPA_ONNX_ROOT}/bin"
    NO_DEFAULT_PATH
    REQUIRED
)

find_file(ONNXRUNTIME_DLL
    NAMES onnxruntime.dll
    PATHS "${SHERPA_ONNX_ROOT}/lib" "${SHERPA_ONNX_ROOT}/bin"
    NO_DEFAULT_PATH
    REQUIRED
)

find_file(ONNXRUNTIME_PROVIDERS_DLL
    NAMES onnxruntime_providers_shared.dll
    PATHS "${SHERPA_ONNX_ROOT}/lib" "${SHERPA_ONNX_ROOT}/bin"
    NO_DEFAULT_PATH
)

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
        "${SHERPA_ONNX_LIB}"
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

    # Copy required runtime DLLs to target binary directory
    set(DLLS_TO_COPY "${SHERPA_ONNX_DLL}" "${ONNXRUNTIME_DLL}")
    if(ONNXRUNTIME_PROVIDERS_DLL)
        list(APPEND DLLS_TO_COPY "${ONNXRUNTIME_PROVIDERS_DLL}")
    endif()

    add_custom_command(TARGET sherpa-server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DLLS_TO_COPY}
            $<TARGET_FILE_DIR:sherpa-server>
    )
endif()

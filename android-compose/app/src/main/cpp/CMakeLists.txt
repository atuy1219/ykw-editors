cmake_minimum_required(VERSION 3.22.1)
project(YkwSaveCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(CRYPTOPP_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CRYPTOPP_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(CRYPTOPP_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    cryptopp-cmake
    GIT_REPOSITORY https://github.com/abdes/cryptopp-cmake.git
    GIT_TAG CRYPTOPP_8_9_0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(cryptopp-cmake)

if(TARGET cryptopp::cryptopp)
    set(YKW_CRYPTOPP_TARGET cryptopp::cryptopp)
elseif(TARGET cryptopp-static)
    set(YKW_CRYPTOPP_TARGET cryptopp-static)
elseif(TARGET cryptopp)
    set(YKW_CRYPTOPP_TARGET cryptopp)
else()
    message(FATAL_ERROR "Crypto++ target was not created")
endif()

add_library(ykw_save_core SHARED native_save_core.cpp)
# cryptopp-cmake exposes its generated `cryptopp` header directory itself.
# The legacy code and this JNI core use the conventional <cryptopp/...> form,
# so also expose the parent build directory as an include root.
target_include_directories(ykw_save_core PRIVATE "${CMAKE_BINARY_DIR}/_deps/cryptopp-cmake-build")
target_link_libraries(ykw_save_core PRIVATE ${YKW_CRYPTOPP_TARGET})
target_compile_options(ykw_save_core PRIVATE -Wall -Wextra -Wpedantic)

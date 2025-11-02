cmake_minimum_required(VERSION 3.28)

# to use vcpkg, export CMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake

project(td365)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# clang has no documentation installed on MacOS
# https://releases.llvm.org/4.0.0/tools/clang/docs/DiagnosticsReference.html#wall
add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wconversion -fno-omit-frame-pointer)

file(GLOB_RECURSE SOURCES src/*.cpp)

# Create an OBJECT library to avoid rebuilding sources for both static and shared
add_library(td365_obj OBJECT ${SOURCES})

# Set position independent code for the OBJECT library (required for shared libraries)
set_target_properties(td365_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Create shared and static libraries from the object library
add_library(td365 SHARED $<TARGET_OBJECTS:td365_obj>)
add_library(td365_static STATIC $<TARGET_OBJECTS:td365_obj>)

# Set version information
set_target_properties(td365 PROPERTIES
        VERSION 1.0.0
        SOVERSION 1
)
set_target_properties(td365_static PROPERTIES OUTPUT_NAME td365)

set(Boost_VERBOSE ON)
set(Boost_NO_BOOST_CMAKE ON)
set(Boost_DEBUG ON)
set(Boost_USE_STATIC_LIBS OFF) # use shared libs for debian package
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF) # do not look for boost libraries linked against static C++ std lib

find_package(Boost REQUIRED COMPONENTS thread iostreams url)
find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

find_package(spdlog CONFIG REQUIRED)

# Set properties and dependencies on the OBJECT library
target_include_directories(td365_obj PUBLIC include ${Boost_INCLUDE_DIRS})
target_link_libraries(td365_obj PUBLIC ${Boost_LIBRARIES} ZLIB::ZLIB OpenSSL::SSL OpenSSL::Crypto nlohmann_json::nlohmann_json spdlog::spdlog_header_only)

# The shared and static libraries inherit from the OBJECT library
target_include_directories(td365 PUBLIC include ${Boost_INCLUDE_DIRS})
target_link_libraries(td365 PUBLIC ${Boost_LIBRARIES} ZLIB::ZLIB OpenSSL::SSL OpenSSL::Crypto nlohmann_json::nlohmann_json spdlog::spdlog_header_only)

target_include_directories(td365_static PUBLIC include ${Boost_INCLUDE_DIRS})
target_link_libraries(td365_static PUBLIC ${Boost_LIBRARIES} ZLIB::ZLIB OpenSSL::SSL OpenSSL::Crypto nlohmann_json::nlohmann_json spdlog::spdlog_header_only)

add_executable(trade_test examples/trade_test.cpp)
target_include_directories(trade_test PUBLIC include)
target_link_libraries(trade_test PUBLIC td365)

target_compile_options(trade_test PUBLIC -fsanitize=address -g -O1)
target_link_options(trade_test PUBLIC -fsanitize=address)


# ----------------------------------------------------------------------------
enable_testing()

# requires Catch2 to be installed via vcpkg: vcpkg install catch2
find_package(Catch2 CONFIG REQUIRED)

add_executable(td365_tests
        tests/test_parsing.cpp
        tests/test_ws_reconnect.cpp
)

target_link_libraries(td365_tests PRIVATE
        td365_static
        Catch2::Catch2WithMain
)

# auto-discover any Catch2 TEST_CASEs (optional)
include(CTest)
include(Catch)

catch_discover_tests(td365_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        REPORT_QUIET
)

# Install targets
install(TARGETS td365 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS td365_static ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Install headers
file(GLOB HEADERS include/td365/*.h)
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/td365)

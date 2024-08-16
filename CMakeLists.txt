cmake_minimum_required(VERSION 3.28.1)
project(BillionRowChallenge VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
    # If release mode
    if(CMAKE_BUILD_TYPE MATCHES Release)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Ox")
    endif()
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=array-bounds")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

include(${PROJECT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)

find_package(unordered_dense CONFIG REQUIRED)
find_package(mio CONFIG REQUIRED)
find_path(STRINGZILLA_INCLUDE_DIRS "stringzilla/experimental.h")

add_executable(AggregateData src/AggregateData.cpp)
target_link_libraries(AggregateData PRIVATE unordered_dense::unordered_dense)
target_link_libraries(AggregateData PRIVATE mio::mio mio::mio-headers)
if(WIN32)
    target_link_libraries(AggregateData PRIVATE mio::mio_min_winapi mio::mio_full_winapi)
endif()
target_include_directories(AggregateData PRIVATE ${STRINGZILLA_INCLUDE_DIRS})
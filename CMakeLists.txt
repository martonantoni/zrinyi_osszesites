cmake_minimum_required(VERSION 3.20)

project(ZrinyiOsszesites VERSION 1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /FC")  # full path in error messages

find_package(CURL REQUIRED)


file(GLOB SOURCES *.cpp)	  
add_executable(zrinyi_osszesites ${SOURCES})
target_link_libraries(zrinyi_osszesites PRIVATE CURL::libcurl)


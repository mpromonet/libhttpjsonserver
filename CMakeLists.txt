cmake_minimum_required (VERSION 3.0.0)
 
project (libhttpservertest)

set (CMAKE_CXX_STANDARD 11)

add_executable (${PROJECT_NAME} main.cpp)

# thread
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package (Threads)
target_link_libraries (${PROJECT_NAME} Threads::Threads) 

# civetweb
add_definitions(-DUSE_WEBSOCKET -DNO_SSL_DL)
add_library (civetweb STATIC civetweb/src/civetweb.c civetweb/src/CivetServer.cpp)	
target_link_libraries (${PROJECT_NAME} civetweb ssl crypto) 
include_directories(civetweb PRIVATE civetweb/include) 

# libhttpserver
FILE(GLOB LIBSOURCE src/*.cpp)
add_library (libhttpserver STATIC ${LIBSOURCE})	
target_include_directories(libhttpserver PUBLIC inc) 
target_link_libraries (${PROJECT_NAME} libhttpserver) 

# jsoncpp
add_subdirectory(jsoncpp)
target_link_libraries (${PROJECT_NAME} jsoncpp_lib) 
include_directories(jsoncpp_lib PRIVATE jsoncpp/include) 
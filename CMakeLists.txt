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

# libhttpserver
FILE(GLOB LIBSOURCE src/*.cpp civetweb/src/civetweb.c civetweb/src/CivetServer.cpp jsoncpp/src/lib_json/*.cpp)
add_library (httpjsonserver STATIC ${LIBSOURCE})	
target_include_directories(httpjsonserver PUBLIC inc civetweb/include jsoncpp/include) 
target_link_libraries (${PROJECT_NAME} httpjsonserver ssl crypto)  


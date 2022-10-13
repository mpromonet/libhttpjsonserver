cmake_minimum_required (VERSION 3.0.0)
 
project (httpjsonservertest)

set (CMAKE_CXX_STANDARD 11)

# civetweb
add_definitions(-DUSE_WEBSOCKET -DNO_SSL_DL)

# libhttpserver
FILE(GLOB LIBSOURCE src/*.cpp civetweb/src/civetweb.c civetweb/src/CivetServer.cpp jsoncpp/src/lib_json/*.cpp)
add_library (httpjsonserver STATIC ${LIBSOURCE})	
target_include_directories(httpjsonserver PUBLIC inc civetweb/include jsoncpp/include) 

# thread
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package (Threads)
target_link_libraries (httpjsonserver Threads::Threads ssl crypto)  

# test program
add_executable (${PROJECT_NAME} main.cpp)
target_link_libraries (${PROJECT_NAME} httpjsonserver)  


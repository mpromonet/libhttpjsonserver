cmake_minimum_required (VERSION 3.0.0)
 
project (httpjsonservertest)

set (CMAKE_CXX_STANDARD 11)
option (WITH_SSL "Enable SSL support" ON)

# civetweb
add_definitions(-DUSE_WEBSOCKET)
if (WITH_SSL)
  add_definitions(-DNO_SSL_DL -DOPENSSL_API_1_1)  
else()
  add_definitions(-DNO_SSL)  
endif()

# libhttpserver
FILE(GLOB LIBSOURCE src/*.cpp civetweb/src/civetweb.c civetweb/src/CivetServer.cpp jsoncpp/src/lib_json/*.cpp)
add_library (httpjsonserver STATIC ${LIBSOURCE})	
target_include_directories(httpjsonserver PUBLIC inc civetweb/include jsoncpp/include) 

# thread
find_package (Threads)
target_link_libraries (httpjsonserver PUBLIC Threads::Threads)  
if (WITH_SSL)
	target_link_libraries (httpjsonserver PUBLIC ssl crypto)  
endif()

# test program
add_executable (${PROJECT_NAME} main.cpp)
target_link_libraries (${PROJECT_NAME} httpjsonserver)  


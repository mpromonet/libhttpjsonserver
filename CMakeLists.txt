cmake_minimum_required (VERSION 3.5)
 
project (httpjsonservertest)

set (CMAKE_CXX_STANDARD 11)
option (WITH_SSL "Enable SSL support" ON)

# libhttpjsonserver
FILE(GLOB LIBSOURCE src/*.cpp civetweb/src/civetweb.c civetweb/src/CivetServer.cpp jsoncpp/src/lib_json/*.cpp)
add_definitions(-DUSE_WEBSOCKET)
add_library (httpjsonserver STATIC ${LIBSOURCE})	
target_include_directories(httpjsonserver PUBLIC inc civetweb/include jsoncpp/include) 

# thread
find_package (Threads)
target_link_libraries (httpjsonserver PUBLIC Threads::Threads)  

# ssl
if (WITH_SSL)
  add_definitions(-DNO_SSL_DL)
  find_package(OpenSSL)
  if(OPENSSL_FOUND)
      message(STATUS "OpenSSL found: ${OPENSSL_INCLUDE_DIR}")
      target_include_directories(httpjsonserver PUBLIC ${OPENSSL_INCLUDE_DIR})
      target_link_libraries(httpjsonserver PUBLIC ${OPENSSL_LIBRARIES})
  else()
      message(STATUS "OpenSSL not found try to guess")
      add_definitions(-DOPENSSL_API_1_1)  
      target_link_libraries (httpjsonserver PUBLIC ssl crypto)
  endif()
else()
  add_definitions(-DNO_SSL)  
endif()

# test program
add_executable (${PROJECT_NAME} main.cpp)
target_link_libraries (${PROJECT_NAME} httpjsonserver)  


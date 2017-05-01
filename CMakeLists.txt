cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)
project(RealmOfAesirGateway CXX)
#set(CMAKE_CXX_STANDARD 17) #not supported yet :(
#set(CMAKE CXX STANDARD REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# only support linux for now
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DELPP_THREAD_SAFE -Wall -Wno-unused-variable -Wno-long-long -Wfloat-equal -pedantic -std=c++17")
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -ggdb -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-g -O2")

include_directories("${PROJECT_SOURCE_DIR}")

file(GLOB_RECURSE PROJECT_SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)
file(GLOB EASYLOGGING_SOURCE ${CMAKE_SOURCE_DIR}/external/common_backend/external/common/external/easyloggingpp/src/easylogging++.cc)
add_executable(RealmOfAesirGateway ${EASYLOGGING_SOURCE} ${PROJECT_SOURCES} )

include_directories("${CMAKE_SOURCE_DIR}/external/common_backend/src")
include_directories("${CMAKE_SOURCE_DIR}/external/common_backend/external/common/src")
include_directories("${CMAKE_SOURCE_DIR}/external/common_backend/external/librdkafka/src-cpp")
include_directories("${CMAKE_SOURCE_DIR}/external/common_backend/external/librdkafka/src")
include_directories("${CMAKE_SOURCE_DIR}/external/common_backend/external/common/external/easyloggingpp/src")
include_directories("${CMAKE_SOURCE_DIR}/external/common_backend/external/common/external/cereal/include")
include_directories("${CMAKE_SOURCE_DIR}/external/common_backend/external/common/external/di/include")
include_directories("${CMAKE_SOURCE_DIR}/external/common_backend/external/libpqxx/include")
include_directories("${CMAKE_SOURCE_DIR}/external/uWebSockets/src")
include_directories("${CMAKE_SOURCE_DIR}/external/common_backend/external/common/external/json/src")

find_library(LIBRDKAFKA_LIBRARY rdkafka HINTS ${CMAKE_SOURCE_DIR}/external/common_backend/external/librdkafka/src)
find_library(LIBRDKAFKAPP_LIBRARY rdkafka++ HINTS ${CMAKE_SOURCE_DIR}/external/common_backend/external/librdkafka/src-cpp)
find_library(UWEBSOCKET_LIBRARY uWS HINTS ${CMAKE_SOURCE_DIR}/external/uWebSockets)
find_library(PQXX_LIBRARY pqxx HINTS ${CMAKE_SOURCE_DIR}/external/common_backend/external/libpqxx/src/.libs)
find_library(LIBROA_COMMON_LIBRARY RealmOfAesirCommon HINTS ${CMAKE_SOURCE_DIR}/external/common_backend/external/common/bin)
find_library(LIBROA_COMMON_BACKEND_LIBRARY RealmOfAesirCommonBackend HINTS ${CMAKE_SOURCE_DIR}/external/common_backend/bin)

if(NOT LIBRDKAFKA_LIBRARY)
    message(FATAL_ERROR "librdkafka not found")
endif()

if(NOT LIBRDKAFKAPP_LIBRARY)
    message(FATAL_ERROR "librdkafkapp not found")
endif()

if(NOT UWEBSOCKET_LIBRARY)
    message(FATAL_ERROR "libuws not found")
endif()

if(NOT PQXX_LIBRARY)
    message(FATAL_ERROR "libpqxx not found")
endif()

if(NOT LIBROA_COMMON_LIBRARY)
    message(FATAL_ERROR "libroa not found")
endif()

if(NOT LIBROA_COMMON_BACKEND_LIBRARY)
    message(FATAL_ERROR "libroa_backend not found")
endif()

target_link_libraries(RealmOfAesirGateway PUBLIC ${LIBROA_COMMON_LIBRARY})
target_link_libraries(RealmOfAesirGateway PUBLIC ${LIBROA_COMMON_BACKEND_LIBRARY})
target_link_libraries(RealmOfAesirGateway PUBLIC ${LIBRDKAFKAPP_LIBRARY})
target_link_libraries(RealmOfAesirGateway PUBLIC ${LIBRDKAFKA_LIBRARY})
target_link_libraries(RealmOfAesirGateway PUBLIC ${UWEBSOCKET_LIBRARY})
target_link_libraries(RealmOfAesirGateway PUBLIC ${PQXX_LIBRARY})

find_package(PostgreSQL REQUIRED)
include_directories(${PostgreSQL_INCLUDE_DIRS})
target_link_libraries(RealmOfAesirGateway PUBLIC ${PostgreSQL_LIBRARIES})

find_package( ZLIB REQUIRED )
include_directories( ${ZLIB_INCLUDE_DIRS} )
target_link_libraries(RealmOfAesirGateway PUBLIC ${ZLIB_LIBRARIES} )

find_package (Threads)
target_link_libraries (RealmOfAesirGateway PUBLIC ${CMAKE_THREAD_LIBS_INIT})

find_package(OpenSSL REQUIRED)
target_link_libraries (RealmOfAesirGateway PUBLIC ${OPENSSL_LIBRARIES})
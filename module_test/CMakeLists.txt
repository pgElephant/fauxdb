find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPQ REQUIRED libpq)
include_directories(${LIBPQ_INCLUDE_DIRS})
link_directories(${LIBPQ_LIBRARY_DIRS})

pkg_check_modules(LIBBSON REQUIRED bson2)
include_directories(${LIBBSON_INCLUDE_DIRS})
link_directories(${LIBBSON_LIBRARY_DIRS})

pkg_check_modules(MONGOC REQUIRED mongoc2)
include_directories(${MONGOC_INCLUDE_DIRS})
link_directories(${MONGOC_LIBRARY_DIRS})
cmake_minimum_required(VERSION 3.16)
project(FauxDBNetworkTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(../include)
include_directories(../include/network)
include_directories(../include/protocol)
include_directories(../include/protocol/query_response)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPQ REQUIRED libpq)
include_directories(${LIBPQ_INCLUDE_DIRS})
link_directories(${LIBPQ_LIBRARY_DIRS})

pkg_check_modules(LIBBSON REQUIRED bson2)
include_directories(${LIBBSON_INCLUDE_DIRS})
link_directories(${LIBBSON_LIBRARY_DIRS})


# Add FauxDB network sources
set(FAUXDB_NETWORK_SOURCES
	../src/network/CTcp.cpp
	../src/network/CNetwork.cpp
	../src/network/CThread.cpp
	../src/protocol/CDocumentProtocolHandler.cpp
	../src/protocol/CDocumentWireProtocol.cpp
	../src/protocol/query_response/CQueryTranslator.cpp
	../src/protocol/query_response/CResponseBuilder.cpp
	../src/protocol/CBsonType.cpp
)

add_executable(network_server network_server.cpp ${FAUXDB_NETWORK_SOURCES})
add_executable(network_client network_client.cpp ${FAUXDB_NETWORK_SOURCES})

target_link_libraries(network_server ${LIBPQ_LIBRARIES} ${LIBBSON_LIBRARIES})
target_link_libraries(network_client ${LIBPQ_LIBRARIES} ${LIBBSON_LIBRARIES})

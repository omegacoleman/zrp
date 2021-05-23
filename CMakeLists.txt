cmake_minimum_required(VERSION 3.1)
project(arkio)

set(CMAKE_CXX_STANDARD 20)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts -stdlib=libc++")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
endif()

find_package (Threads REQUIRED)
find_package(fmt REQUIRED)
find_package (Boost 1.75 REQUIRED COMPONENTS system json)

add_executable(zclient ${PROJECT_SOURCE_DIR}/src/zclient.cpp)
target_include_directories(zclient PUBLIC ${PROJECT_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})
target_link_libraries(zclient PUBLIC fmt::fmt Threads::Threads ${Boost_LIBRARIES})

add_executable(zserver ${PROJECT_SOURCE_DIR}/src/zserver.cpp)
target_include_directories(zserver PUBLIC ${PROJECT_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})
target_link_libraries(zserver PUBLIC fmt::fmt Threads::Threads ${Boost_LIBRARIES})


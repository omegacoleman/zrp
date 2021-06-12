cmake_minimum_required(VERSION 3.2)

include("cmake/HunterGate.cmake")
HunterGate(
    URL "https://github.com/cpp-pm/hunter/archive/v0.23.299.tar.gz"
    SHA1 "3d215b4bfac80824778fe9a93510e5f651d79e3a"
)

project(arkio)

set(CMAKE_CXX_STANDARD 20)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
endif()

hunter_add_package(Boost COMPONENTS system json)
hunter_add_package(fmt)

set(Boost_USE_STATIC_LIBS ON)

find_package (Threads REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package (Boost 1.75.0 CONFIG REQUIRED COMPONENTS system json)

set(ZRP_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include)
set(ZRP_LIBRARIES Threads::Threads fmt::fmt-header-only Boost::boost Boost::system Boost::json)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	set(ZRP_LIBRARIES ${ZRP_LIBRARIES} bcrypt)
endif()

add_executable(zclient ${PROJECT_SOURCE_DIR}/src/zclient.cpp)
target_include_directories(zclient PUBLIC ${ZRP_INCLUDE_DIRS})
target_link_libraries(zclient PUBLIC ${ZRP_LIBRARIES})

add_executable(zserver ${PROJECT_SOURCE_DIR}/src/zserver.cpp)
target_include_directories(zserver PUBLIC ${ZRP_INCLUDE_DIRS})
target_link_libraries(zserver PUBLIC ${ZRP_LIBRARIES})

install(TARGETS zclient zserver RUNTIME DESTINATION bin)
include(InstallRequiredSystemLibraries)


# "Are there any good C/CPP Build systems?"
# *Shakes Magic 8 Ball*
# "Very doubtful"

cmake_minimum_required(VERSION 3.18.4)
project(CanRed VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 11)
# -Wno-psabi -> I am not entirely sure why, but compiling json.hpp on 
#               a raspberry pi, entirely using gcc 10^, creates ABI warnings
#               which do not appear when compiling for x86-64
set(CMAKE_CXX_FLAGS "-Wall -Werror -Wextra -Wpedantic -pthread -Wno-psabi")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -ggdb -fsanitize=address") # Overwrites "-g"
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG") # Overwrites "-03 -DNDEBUG"

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

set(SOURCES
    # Local files
    ${PROJECT_SOURCE_DIR}/src/main.cpp
    ${PROJECT_SOURCE_DIR}/lib/CanManager/CanManager.cpp
    ${PROJECT_SOURCE_DIR}/lib/EventManager/EventManager.cpp
    ${PROJECT_SOURCE_DIR}/lib/Interfaces/SerialInterface.cpp
    ${PROJECT_SOURCE_DIR}/lib/CanManager/LongFrameHandler.cpp
    ${PROJECT_SOURCE_DIR}/lib/CanManager/ACKHandler.cpp
    ${PROJECT_SOURCE_DIR}/lib/SocketWatcher/SocketWatcher.cpp
    
    ${PROJECT_SOURCE_DIR}/lib/Database/Database.cpp
    ${PROJECT_SOURCE_DIR}/lib/Database/SqliteIterator.cpp
    ${PROJECT_SOURCE_DIR}/lib/Database/SqliteQuery.cpp
    ${PROJECT_SOURCE_DIR}/lib/Database/SqliteValue.cpp

    # Common Files
    ${PROJECT_SOURCE_DIR}/../Common/Events/Event.cpp
    ${PROJECT_SOURCE_DIR}/../Common/CAN/CanFrame.cpp
    ${PROJECT_SOURCE_DIR}/../Common/CAN/CanID.cpp
    ${PROJECT_SOURCE_DIR}/../Common/AnyType/AnyType.cpp
   )

add_executable(CanRed ${SOURCES})

add_definitions(-DCANRED)


# Local files
include_directories("${PROJECT_SOURCE_DIR}/lib/Database")
include_directories("${PROJECT_SOURCE_DIR}/lib/CanManager")
include_directories("${PROJECT_SOURCE_DIR}/lib/Json")
include_directories("${PROJECT_SOURCE_DIR}/lib/Utils")
include_directories("${PROJECT_SOURCE_DIR}/lib/EventManager")
include_directories("${PROJECT_SOURCE_DIR}/lib/Interfaces")
include_directories("${PROJECT_SOURCE_DIR}/lib/SocketWatcher")
include_directories("${PROJECT_SOURCE_DIR}/lib/Module")

# Common Files
include_directories("${PROJECT_SOURCE_DIR}/../Common/CAN")
include_directories("${PROJECT_SOURCE_DIR}/../Common/Events")
include_directories("${PROJECT_SOURCE_DIR}/../Common/AnyType")
include_directories("${PROJECT_SOURCE_DIR}/../Common/Interfaces")
include_directories("${PROJECT_SOURCE_DIR}/../Common/Util")
include_directories("${PROJECT_SOURCE_DIR}/../Common/Platform")

# Sqlite3 requires libdl for loading extentions vv
target_link_libraries(CanRed ${CMAKE_DL_LIBS} ${CONAN_LIBS})

# Crosscompilling
if(CROSSCOMPILLING)
    # NOT IMPLEMENTED CORRECTLY 
    # link_directories(${rootfs_dir}/arm-linux-gnueabihf/)
    # include_directories(${rootfs_dir}/usr/include/arm-linux-gnueabihf)
    # link_directories(${rootfs_dir}/usr/lib/)
    # target_link_libraries(CanRed ${CMAKE_DL_LIBS} -static)
endif()

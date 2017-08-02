# cmake mini required version 2.6 
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
# set project name 
PROJECT(websocket_server)
MESSAGE(STATUS "Current Projec Name: ${PROJECT_NAME}")
# display current system information
MESSAGE(STATUS "Current System Info: ${CMAKE_SYSTEM} ${CMAKE_SYSTEM_PROCESSOR}")
# check the dependency files and libraries
# check pthread.h and libpthread.so

FIND_FILE(HAVE_PTHREAD_H pthread.h "/usr/include")
IF(NOT HAVE_PTHREAD_H)
	MESSAGE(FATAL_ERROR "pthread.h not found")
ENDIF(NOT HAVE_PTHREAD_H)

# gcc/g++ options
SET(CMAKE_BUILD_TYPE "Debug")

SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
ADD_DEFINITIONS(-g -Wall -Wparentheses -fno-delete-null-pointer-checks)

# add subdirectory
ADD_SUBDIRECTORY(src)

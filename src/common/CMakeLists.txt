MESSAGE(STATUS "This is SOURCE dir " ${CMAKE_CURRENT_SOURCE_DIR})
MESSAGE(STATUS "This is BINARY dir " ${CMAKE_CURRENT_BINARY_DIR})

FILE(GLOB SRC 					*.cpp *.h)
FILE(GLOB SRC_JSON				json/*.h json/*.cpp json/*.c)
FILE(GLOB SRC_MONGODB			mongodb/*.h mongodb/*.cpp mongodb/*.c)

SOURCE_GROUP("" 				FILES ${SRC})
SOURCE_GROUP("json" 			FILES ${SRC_JSON})
SOURCE_GROUP("mongodb"			FILES ${SRC_MONGODB})

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR/common}
)

LINK_DIRECTORIES(${PROJECT_BINARY_DIR/common}
)

ADD_LIBRARY(common STATIC 
		${SRC}
		${SRC_JSON}
		${SRC_MONGODB}		
)
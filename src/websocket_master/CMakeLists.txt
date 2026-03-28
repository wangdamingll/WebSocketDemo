MESSAGE(STATUS "This is SOURCE dir " ${CMAKE_CURRENT_SOURCE_DIR})
MESSAGE(STATUS "This is BINARY dir " ${CMAKE_CURRENT_BINARY_DIR})

FILE(GLOB SRC 					*.cpp *.h)
FILE(GLOB SRC_TOOLS 		    Tools/*.cpp Tools/*.h)

SOURCE_GROUP("" 				FILES ${SRC})

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}
)

LINK_DIRECTORIES(${PROJECT_BINARY_DIR}/common/
			/usr/local/lib/
)

add_executable(websocketserver 
			${SRC}
            ${SRC_TOOLS}
)

add_dependencies(websocketserver common)

TARGET_LINK_LIBRARIES(websocketserver
					${CMAKE_BINARY_DIR}/src/common/libcommon.a
					rt 
                    z
                    
)

INSTALL(TARGETS websocketserver RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/../bin)

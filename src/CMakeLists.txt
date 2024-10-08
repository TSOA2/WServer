set(INC_DIR ${CMAKE_SOURCE_DIR}/src/include)
set(INC_FILES
	${INC_DIR}/log.h
	${INC_DIR}/http.h
	${INC_DIR}/config.h
	${INC_DIR}/resource.h
)
set(SRC_FILES
	server.c
	log.c
	http.c
	resource.c
)

add_executable(wserver ${SRC_FILES} ${INC_FILES})
target_include_directories(wserver PRIVATE ${INC_DIR})

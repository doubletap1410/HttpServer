project(HttpServer)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(HEADERS common.h)

aux_source_directory(. SRC_LIST)
#add_executable(${PROJECT_NAME} ${SRC_LIST} ${HEADERS})
add_executable("final" ${SRC_LIST} ${HEADERS})


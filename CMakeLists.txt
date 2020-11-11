cmake_minimum_required(VERSION "3.7.1")
project("BranchNBound")

SET(CMAKE_C_COMPILER mpicc)
SET(CMAKE_CXX_COMPILER mpicxx)
SET(CMAKE_CXX_STANDARD 17)

find_package(MPI REQUIRED)

include_directories(include)
file(GLOB SRC
		    "include/*.h"
)

#add_executable("${PROJECT_NAME}" ${SRC})
add_library("${PROJECT_NAME}" INTERFACE)
target_include_directories("${PROJECT_NAME}" INTERFACE include/)

#target_link_libraries(BnB PRIVATE MPI)
#install(TARGETS ${EXECUTABLE_NAME} DESTINATION bin)
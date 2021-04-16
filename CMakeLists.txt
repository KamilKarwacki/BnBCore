cmake_minimum_required(VERSION "3.15.2")
project("BranchNBound" LANGUAGES CXX)

# setting variables which will control how to build
option(BUILD_WITH_OMP 
		"Whether to build with MPI (to run on a cluster)" ON)
option(BUILD_WITH_MPI 
		"Whether to build with OMP (to run in parallel on a single machine)" ON)
option(PACKAGE_TESTING "Build the tests" OFF)

# our only target, a header only library (INTERFACE)
add_library("${PROJECT_NAME}" INTERFACE)

if(BUILD_WITH_MPI)
	find_package(MPI REQUIRED)
	message(STATUS "Run: ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} ${MPIEXEC_MAX_NUMPROCS} ${MPIEXEC_PREFLAGS} EXECUTABLE ${MPIEXEC_POSTFLAGS} ARGS")
	target_link_libraries("${PROJECT_NAME}" INTERFACE MPI::MPI_CXX)
endif()

if(BUILD_WITH_OMP)
	find_package(OpenMP)
	if(OpenMP_CXX_FOUND)
		message("set OMP as interface dependence")
		target_link_libraries(${PROJECT_NAME} INTERFACE OpenMP::OpenMP_CXX)
	endif()
endif()

# testing stuff ----------------------
if(PACKAGE_TESTING)
	enable_testing()
	include(GoogleTest)
	add_subdirectory(tests)
endif()
# ------------------------------------

#add_subdirectory(extern/lp_solve_5.5)
#target_link_libraries(${PROJECT_NAME} INTERFACE LP)
#find_package(Eigen3 3.3 REQUIRED NO_MODULE)
#target_link_libraries(${PROJECT_NAME} INTERFACE Eigen3::Eigen)

target_include_directories(
		${PROJECT_NAME}
		INTERFACE $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/include/Core>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_include_directories(
		${PROJECT_NAME}
		INTERFACE $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/include/Shared>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_include_directories(
		${PROJECT_NAME}
		INTERFACE $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/include/Distributed>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)


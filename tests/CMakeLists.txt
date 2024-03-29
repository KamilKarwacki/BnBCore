cmake_minimum_required(VERSION "3.15.2")

macro(package_add_test TESTNAME)
	# create an exectuable in which the tests will be stored
	add_executable(${TESTNAME} ${ARGN})
	# link the Google test infrastructure, mocking library, and a default main fuction to
	# the test executable.  Remove g_test_main if writing your own main function.
	target_link_libraries(${TESTNAME} gtest gmock gtest_main BranchNBound)
	# gtest_discover_tests replaces gtest_add_tests,
	# see https://cmake.org/cmake/help/v3.10/module/GoogleTest.html for more options to pass to it
	gtest_discover_tests(${TESTNAME}
		# set a working directory so your project root so that you can find test data via paths relative to the project root
		WORKING_DIRECTORY ${PROJECT_DIR}
		PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_DIR}"
	)
	set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
endmacro()

function(add_mpi_test TESTNAME)
  include_directories(${MY_TESTING_INCLUDES})
  # My test are all called name_test.cpp
  add_executable(${TESTNAME} ${ARGN})
  # libraries we need for the test
  set(LIBRARIES BranchNBound)
  target_link_libraries(${TESTNAME} gtest gmock gtest_main ${LIBRARIES})

  set(test_parameters "${MPIEXEC_NUMPROC_FLAG} 3 ./${TESTNAME}")
  #message(${test_parameters})
  #message(${MPIEXEC})
  add_test(NAME ${TESTNAME} COMMAND ${MPIEXEC} ${test_parameters})
endfunction(add_mpi_test)



add_subdirectory("${PROJECT_SOURCE_DIR}/extern/googletest" "extern/googletest")
package_add_test(EncoderTest EncoderTests.cpp)
package_add_test(OpenMPTest  OpenMPKnapsackTest.cpp)
add_mpi_test(MPITest         MPIKnapsackTest.cpp)


cmake_minimum_required(VERSION "3.7.1")

set(This EncoderTest)

set(Sources
		EncoderTests.cpp
)

add_executable(${This} ${Sources})

target_link_libraries(${This} PUBLIC
		gtest_main
		BranchNBound
)

add_test(
		NAME ${This}
		COMMAND ${This}
)

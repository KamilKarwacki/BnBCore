#include<gtest/gtest.h>
#include "Knapsack.h"
#include "Base.h"
#include "BnB_OMP_Solver.h"
#include <limits>



TEST(OMPKnapsack1, DemonstratedGTestMacros)
{
	Solver_omp<BnB::Knapsack::Consts, BnB::Knapsack::Params, int> solver;
	solver.setNumThreads(4);

	BnB::Knapsack::Consts TestConsts;
	std::get<0>(TestConsts) = {3,3,3,3,3};
	std::get<1>(TestConsts) = {10,2,10,4,10};
	std::get<2>(TestConsts) = 10;	

	auto result = solver.Maximize(BnB::Knapsack::GenerateToyProblem(), TestConsts);
	EXPECT_EQ(std::get<2>(result),  30) <<  "Weight does not match";
}	


TEST(OMPKnapsack2, DemonstratedGTestMacros)
{
	Solver_omp<BnB::Knapsack::Consts, BnB::Knapsack::Params, int> solver;
	solver.setNumThreads(4);

	BnB::Knapsack::Consts TestConsts;
	std::get<0>(TestConsts) = {3,3,3,3,3};
	std::get<1>(TestConsts) = {10,2,10,4,10};
	std::get<2>(TestConsts) = 100;	

	auto result = solver.Maximize(BnB::Knapsack::GenerateToyProblem(), TestConsts);
	EXPECT_EQ(std::get<2>(result),  36) <<  "Weight does not match";
}	

TEST(OMPKnapsack3, DemonstratedGTestMacros)
{
	Solver_omp<BnB::Knapsack::Consts, BnB::Knapsack::Params, int> solver;
	solver.setNumThreads(4);

	BnB::Knapsack::Consts TestConsts;
	std::get<0>(TestConsts) = {3,3,3,3,3};
	std::get<1>(TestConsts) = {10,2,10,4,10};
	std::get<2>(TestConsts) = 1;	

	auto result = solver.Maximize(BnB::Knapsack::GenerateToyProblem(), TestConsts);
	EXPECT_EQ(std::get<2>(result), 0) <<  "Weight does not match";
}	

int main(int argc, char* argv[])
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

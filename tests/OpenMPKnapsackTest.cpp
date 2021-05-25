#include<gtest/gtest.h>
#include "Knapsack.h"
#include "Base.h"
#include "BnB_OMP_Solver.h"


TEST(OMPKnapsack, someItemsFit)
{
	BnB::Solver_OMP<BnB::Knapsack::Consts, BnB::Knapsack::Params, int> solver;
	solver.SetNumThreads(4);

	BnB::Knapsack::Consts TestConsts;
	std::get<0>(TestConsts) = {3,3,3,3,3};
	std::get<1>(TestConsts) = {10,2,10,4,10};
	std::get<2>(TestConsts) = 10;	

	auto result = std::get<0>(solver.Maximize(BnB::Knapsack::GenerateToyProblem(), TestConsts));
	int cost = 0;
	for(const auto& item : result)
		cost += std::get<1>(TestConsts)[item];
	EXPECT_EQ(cost,  30) <<  "Weight does not match";
}	


TEST(OMPKnapsack, allItemsFit)
{
	BnB::Solver_OMP<BnB::Knapsack::Consts, BnB::Knapsack::Params, int> solver;
	solver.SetNumThreads(4);

	BnB::Knapsack::Consts TestConsts;
	std::get<0>(TestConsts) = {3,3,3,3,3};
	std::get<1>(TestConsts) = {10,2,10,4,10};
	std::get<2>(TestConsts) = 100;	

	
	auto result = std::get<0>(solver.Maximize(BnB::Knapsack::GenerateToyProblem(), TestConsts));
	int cost = 0;
	for(const auto& item : result)
		cost += std::get<1>(TestConsts)[item];

	EXPECT_EQ(cost,  36) <<  "Weight does not match";
	
}


TEST(OMPKnapsack, NoItemsFit)
{	
	BnB::Solver_OMP<BnB::Knapsack::Consts, BnB::Knapsack::Params, int> solver;
	solver.SetNumThreads(4);

	BnB::Knapsack::Consts TestConsts;
	std::get<0>(TestConsts) = {3,3,3,3,3};
	std::get<1>(TestConsts) = {10,2,10,4,10};
	std::get<2>(TestConsts) = 1;	

	auto result = std::get<0>(solver.Maximize(BnB::Knapsack::GenerateToyProblem(), TestConsts));
	int cost = 0;
		
	for(const auto& item : result)
		cost += std::get<1>(TestConsts)[item];

	EXPECT_EQ(cost,  0) <<  "Weight does not match";	
}

int main(int argc, char* argv[])
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

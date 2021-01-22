#include<gtest/gtest.h>
#include "Knapsack.h"
#include "Base.h"
#include "BnB_MPI_Solver.h"


void TestResult(std::vector<int> result, std::vector<int> costs, int target)
{
	int id;
	MPI_Comm_rank(MPI_COMM_WORLD, &id);
	if(id == 0)
	{	
	int cost = 0;
	for(const auto& item : result)
		{
			cost += costs[item];
		}
	EXPECT_EQ(cost,  target) <<  "Weight does not match";
	}
}


TEST(MPIKnapsack, someItemsFit)
{
	Solver_MPI<BnB::Knapsack::Consts, BnB::Knapsack::Params, int> solver;
	solver.SetScheduler(Scheduler_Type::DEFAULT);

	BnB::Knapsack::Consts TestConsts;
	std::get<0>(TestConsts) = {3,3,3,3,3};
	std::get<1>(TestConsts) = {10,2,10,4,10};
	std::get<2>(TestConsts) = 10;	

	auto result  = std::get<0>(solver.Maximize(BnB::Knapsack::GenerateToyProblem(), TestConsts));
	
	TestResult(result, std::get<1>(TestConsts), 30);	
	
}	


TEST(MPIKnapsack, allItemsFit)
{
	Solver_MPI<BnB::Knapsack::Consts, BnB::Knapsack::Params, int> solver;

	BnB::Knapsack::Consts TestConsts;
	std::get<0>(TestConsts) = {3,3,3,3,3};
	std::get<1>(TestConsts) = {10,2,10,4,10};
	std::get<2>(TestConsts) = 100;	

	auto result  = std::get<0>(solver.Maximize(BnB::Knapsack::GenerateToyProblem(), TestConsts));

	TestResult(result, std::get<1>(TestConsts), 36);	
}

TEST(MPIKnapsack, NoItemsFit)
{
	Solver_MPI<BnB::Knapsack::Consts, BnB::Knapsack::Params, int> solver;

	BnB::Knapsack::Consts TestConsts;
	std::get<0>(TestConsts) = {3,3,3,3,3};
	std::get<1>(TestConsts) = {10,2,10,4,10};
	std::get<2>(TestConsts) = 1;	
	
	auto result  = std::get<0>(solver.Maximize(BnB::Knapsack::GenerateToyProblem(), TestConsts));

	TestResult(result, std::get<1>(TestConsts), 0);	
}

int main(int argc, char* argv[])
{	
    int result = 0;

    if(MPI_Init(&argc, &argv) != MPI_SUCCESS)
		std::cerr << "initialization of MPI failed" << std::endl;
    ::testing::InitGoogleTest(&argc, argv);
	
	int id;
	int num;
	MPI_Comm_rank(MPI_COMM_WORLD, &id);
	MPI_Comm_size(MPI_COMM_WORLD, &num);
	std::cerr << "run with " << num << " processes" << std::endl; 


	::testing::TestEventListeners& listeners =
    ::testing::UnitTest::GetInstance()->listeners();
	if (id != 0) {
    	delete listeners.Release(listeners.default_result_printer());
	}

    result = RUN_ALL_TESTS();
    MPI_Finalize();

    return result;
}



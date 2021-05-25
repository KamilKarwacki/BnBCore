#include<gtest/gtest.h>
#include "../include/Core/Knapsack.h"
#include "../include/Core/Base.h"
#include "../include/Distributed/BnB_MPI_Solver.h"


int GetTotalValue(BnB::Knapsack::Params& t,BnB::Knapsack::Consts& C)
{
    // perform greedy to finish it
    int w = std::get<1>(t);
    int cost = std::get<2>(t);

    float bound = cost;
    for (int j = std::get<0>(t).size(); j < std::get<0>(C).size(); j++) {
        if (w == std::get<2>(C))
            break;
        else if (w + std::get<0>(C)[j] <= std::get<2>(C)) {
            bound += std::get<1>(C)[j];
            w += std::get<0>(C)[j];
        }
    }
    return std::get<3>(t);
}



TEST(MPIKnapsack, someItemsFit)
{
	BnB::Solver_MPI<BnB::Knapsack::Consts, BnB::Knapsack::Params, int> solver;
	solver.SetScheduler(BnB::MPI_Scheduler_Type::PRIORITY);

	BnB::Knapsack::Consts TestConsts;
	std::get<0>(TestConsts) = {3,3,3,3,3};
	std::get<1>(TestConsts) = {10,2,10,4,10};
	std::get<2>(TestConsts) = 10;	

	auto result  = solver.Maximize(BnB::Knapsack::GenerateToyProblem(), TestConsts);

    int id;
    MPI_Comm_rank(MPI_COMM_WORLD, &id);
    if(id == 0)
    {
        EXPECT_EQ(GetTotalValue(result,TestConsts),  30) <<  "Weight does not match";
    }
}


TEST(MPIKnapsack, allItemsFit)
{
	BnB::Solver_MPI<BnB::Knapsack::Consts, BnB::Knapsack::Params, int> solver;

	BnB::Knapsack::Consts TestConsts;
	std::get<0>(TestConsts) = {3,3,3,3,3};
	std::get<1>(TestConsts) = {10,2,10,4,10};
	std::get<2>(TestConsts) = 100;	

	auto result  = solver.Maximize(BnB::Knapsack::GenerateToyProblem(), TestConsts);

    int id;
    MPI_Comm_rank(MPI_COMM_WORLD, &id);
    if(id == 0)
    {
        EXPECT_EQ(GetTotalValue(result,TestConsts),  36) <<  "Weight does not match";
    }
}

TEST(MPIKnapsack, NoItemsFit)
{
	BnB::Solver_MPI<BnB::Knapsack::Consts, BnB::Knapsack::Params, int> solver;

	BnB::Knapsack::Consts TestConsts;
	std::get<0>(TestConsts) = {3,3,3,3,3};
	std::get<1>(TestConsts) = {10,2,10,4,10};
	std::get<2>(TestConsts) = 1;	
	
    auto result  = solver.Maximize(BnB::Knapsack::GenerateToyProblem(), TestConsts);

    int id;
    MPI_Comm_rank(MPI_COMM_WORLD, &id);
    if(id == 0)
    {
        EXPECT_EQ(GetTotalValue(result,TestConsts),  0) <<  "Weight does not match";
    }
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



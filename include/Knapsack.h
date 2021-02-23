#pragma once
#include "Base.h"

// factory function that returns a toy problem
// used for testing solvers, and as an example of how to use the library

namespace BnB::Knapsack
{
    namespace Detail{
        struct item{
            int wt, cost;
            double ratio;
        };

        bool comp(item a, item b){
            return a.ratio > b.ratio;
        }

        std::vector<int> GenerateWeights()
        {
            return {2,2,2,2,8};
        }

        std::vector<int> GenerateCosts()
        {
            return {5,5,5,5,100};
        }

    }

    using Consts = Problem_Constants<
            std::vector<int>, // Weights
            std::vector<int>, // Costs
            int>;             // Capacity of the Bag

    using Params = Subproblem_Parameters<
            std::vector<int> // Taken objects
            >;

    Consts GenerateProblemConstants()
    {
        // NOTE: Params are not needed to be constructed now
        // The initial subproblem will be constructed by Problem_Definition.GetInitialSubproblem()
        // constants will be initialized before solver is run
        Consts consts;
        std::get<0>(consts) = Detail::GenerateWeights();
        std::get<1>(consts) = Detail::GenerateCosts();
        std::get<2>(consts) = 1; // max weight

        return consts;
    }

    Problem_Definition<Consts, Params, int> GenerateToyProblem()
    {
        using namespace Detail;
        // object that will hold the functions called by the solver
        Problem_Definition<Consts, Params, int> KnapsackProblem;

        // you can assume that the subproblem is not feasible yet so it can be split
        KnapsackProblem.SplitSolution = [](const Consts& consts, const Params & params){
            std::vector<Params> subProblems;
            auto objects = std::get<0>(params);
            for(size_t i = 0; i < objects.size(); i++)
            {
                Params subProb;
                std::vector<int> indices;
                for(size_t j = 0; j < objects.size(); j++)
                {
                    if(i != j){
                        indices.push_back(objects[j]);
                    }
                }
                std::get<0>(subProb) = indices;
                subProblems.push_back(subProb);
            }
/*            printProc("splitting into " << subProblems.size());
            for(const auto& sub : subProblems)
                for(const auto& index : std::get<0>(sub))
                    std::cout << index << " ";
                std::cout << std::endl;
*/
            return subProblems;
        };

        KnapsackProblem.GetEstimateForBounds = [](const Consts& consts, const Params& params){
            // calculate the cost as if all items were to fit in the sack
           std::vector<int> objects = std::get<0>(params);
           std::vector<int> costs   = std::get<1>(consts);

           int sumCost = 0;
           for(const auto& object : objects)
               sumCost += costs[object];

           return std::tuple<int,int>(sumCost, 0);
        };

        KnapsackProblem.GetContainedUpperBound = [](const Consts& consts, const Params& params){
            std::vector<int> weights = std::get<0>(consts);
            std::vector<int> costs   = std::get<1>(consts);
            std::vector<int> objects = std::get<0>(params);

            std::vector<item> rem;
            for(const auto& i : std::get<0>(params)){
                item it;
                it.wt = weights[i];
                it.cost = costs[i];
                it.ratio = static_cast<double>(it.cost) / static_cast<double>(it.wt);
                rem.push_back(it);
            }

            int bound = 0;
            int w = 0;
            std::sort(rem.begin(),rem.end(),comp); // decreasing order of ratio of cost to weight;
            for(size_t j=0;j<rem.size() && w + rem[j].wt <= std::get<2>(consts);j++){
                //taking items till weight is exceeded
                bound += rem[j].cost;
                w += rem[j].wt;
            }
            return bound;
        };

        KnapsackProblem.IsFeasible = [](const Consts& consts, const Params& params){
            int NumOfItemsThatFit = 0;
            for(const auto& object : std::get<0>(params))
                if(std::get<0>(consts)[object] <= std::get<2>(consts))
                    NumOfItemsThatFit++;

            //printProc("this config has " << std::get<0>(params).size() << " items in it, but only" << NumOfItemsThatFit <<" fit" );
            if(NumOfItemsThatFit > 0)
                return BnB::FEASIBILITY::FULL;
            else
                return BnB::FEASIBILITY::NONE;
        };


        KnapsackProblem.PrintSolution = [](const Params& params){
            printProc("solution has " << std::get<0>(params).size() << " items");

            for(const auto& el : std::get<0>(params))
            {
                std::cout << "item nr: " << el << std::endl;
            }
        };

        KnapsackProblem.GetInitialSubproblem = [](const Consts& prob){
           Params initial;
           for(int i = 0; i < std::get<0>(prob).size(); i++)
               std::get<0>(initial).push_back(i);
           return initial;
        };

        return KnapsackProblem;
    }
}

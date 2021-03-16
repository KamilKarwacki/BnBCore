#pragma once

#include "Base.h"
#include <random>

// factory function that returns a toy problem
// used for testing solvers, and as an example of how to use the library

namespace BnB::Knapsack {
    namespace Detail {
        struct item {
            int wt, cost;
            double ratio;
        };

        bool comp(item a, item b) {
            return a.ratio > b.ratio;
        }
    }

    using Consts = Problem_Constants<
            std::vector<int>, // Weights
            std::vector<int>, // Costs
            int>;             // Capacity of the Bag

    using Params = Subproblem_Parameters<
            std::vector<int>, // Taken objects
            int, // current weight
            int // current value
    >;


    /*
    // Generates a problem definition which is required by the solver
    Problem_Definition<Consts, Params, int> GenerateFasterProblem();
    Problem_Definition<Consts, Params, int> GenerateToyProblem();

    // randomized generation of knapsack problems
    std::tuple<std::vector<int>, std::vector<int>, int>
        GenerateRandomProblemConstants(const std::pair<int,int>& range, int size, double fitpercentage = 0.5);
        */



    Problem_Definition<Consts, Params, int> GenerateFasterProblem() {
        using namespace Detail;
        // object that will hold the functions called by the solver
        Problem_Definition<Consts, Params, int> KnapsackProblem;

        // you can assume that the subproblem is not feasible yet so it can be split
        KnapsackProblem.SplitSolution = [](const Consts &consts, const Params &params) {
            int ObjectIndex = std::get<0>(params).size();
            Params s1 = params;
            Params s2 = params;
            std::get<0>(s1).push_back(0);
            std::vector<Params> ret;
            ret.push_back(s1); // without taking object at index pos
            if (std::get<0>(consts)[ObjectIndex] + std::get<1>(params) <= std::get<2>(consts)) {
                std::get<1>(s2) += std::get<0>(consts)[ObjectIndex];
                std::get<2>(s2) += std::get<1>(consts)[ObjectIndex];
                std::get<0>(s2).push_back(1);
                ret.push_back(s2); // taking object at index pos
            }
            return ret;
        };

        KnapsackProblem.GetEstimateForBounds = [](const Consts &consts, const Params &params) {
            // calculate the cost as if all items were to fit in the sack

            int ObjectIndex = std::get<0>(params).size();
            int sumCost = std::get<2>(params);
            for (int i = ObjectIndex; i < std::get<0>(consts).size(); i++)
                sumCost += std::get<1>(consts)[i];

            return std::tuple<int, int>(sumCost, 0);
        };

        KnapsackProblem.GetContainedUpperBound = [](const Consts &consts, const Params &params) {
            int w = std::get<1>(params);
            int cost = std::get<2>(params);

            int bound = cost;
            for (int j = std::get<0>(params).size();
                 j < std::get<0>(consts).size() && w + std::get<0>(consts)[j] <= std::get<2>(consts); j++) {
                //taking items till weight is exceeded
                bound += std::get<1>(consts)[j];
                w += std::get<0>(consts)[j];
            }
            return bound;
        };

        KnapsackProblem.IsFeasible = [](const Consts &consts, const Params &params) {
            return std::get<0>(consts).size() != std::get<0>(params).size() ? BnB::FEASIBILITY::PARTIAL : BnB::FEASIBILITY::FULL;
        };


        KnapsackProblem.PrintSolution = [](const Params &params) {
            printProc("solution has " << std::get<0>(params).size() << " items");

            for (const auto &el : std::get<0>(params)) {
                std::cout << "item nr: " << el << std::endl;
            }
        };

        KnapsackProblem.GetInitialSubproblem = [](const Consts &prob) {
            return Params();
        };

        return KnapsackProblem;
    }


    Problem_Definition<Consts, Params, int> GenerateToyProblem() {
        using namespace Detail;
        // object that will hold the functions called by the solver
        Problem_Definition<Consts, Params, int> KnapsackProblem;

        // you can assume that the subproblem is not feasible yet so it can be split
        KnapsackProblem.SplitSolution = [](const Consts &consts, const Params &params) {
            std::vector<Params> subProblems;
            auto objects = std::get<0>(params);
            for (size_t i = 0; i < objects.size(); i++) {
                Params subProb;
                std::vector<int> indices;
                for (size_t j = 0; j < objects.size(); j++) {
                    if (i != j) {
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

        KnapsackProblem.GetEstimateForBounds = [](const Consts &consts, const Params &params) {
            // calculate the cost as if all items were to fit in the sack
            std::vector<int> objects = std::get<0>(params);
            std::vector<int> costs = std::get<1>(consts);

            int sumCost = 0;
            for (const auto &object : objects)
                sumCost += costs[object];

            return std::tuple<int, int>(sumCost, 0);
        };

        KnapsackProblem.GetContainedUpperBound = [](const Consts &consts, const Params &params) {
            std::vector<int> weights = std::get<0>(consts);
            std::vector<int> costs = std::get<1>(consts);
            std::vector<int> objects = std::get<0>(params);

            std::vector<item> rem;
            for (const auto &i : std::get<0>(params)) {
                item it;
                it.wt = weights[i];
                it.cost = costs[i];
                it.ratio = static_cast<double>(it.cost) / static_cast<double>(it.wt);
                rem.push_back(it);
            }

            int bound = 0;
            int w = 0;
            std::sort(rem.begin(), rem.end(), comp); // decreasing order of ratio of cost to weight;
            for (size_t j = 0; j < rem.size() && w + rem[j].wt <= std::get<2>(consts); j++) {
                //taking items till weight is exceeded
                bound += rem[j].cost;
                w += rem[j].wt;
            }
            return bound;
        };

        KnapsackProblem.IsFeasible = [](const Consts &consts, const Params &params) {
            int NumOfItemsThatFit = 0;
            for (const auto &object : std::get<0>(params))
                if (std::get<0>(consts)[object] <= std::get<2>(consts))
                    NumOfItemsThatFit++;

            //printProc("this config has " << std::get<0>(params).size() << " items in it, but only" << NumOfItemsThatFit <<" fit" );
            if (NumOfItemsThatFit > 0)
                return BnB::FEASIBILITY::FULL;
            else
                return BnB::FEASIBILITY::NONE;
        };


        KnapsackProblem.PrintSolution = [](const Params &params) {
            printProc("solution has " << std::get<0>(params).size() << " items");

            for (const auto &el : std::get<0>(params)) {
                std::cout << "item nr: " << el << std::endl;
            }
        };

        KnapsackProblem.GetInitialSubproblem = [](const Consts &prob) {
            Params initial;
            for (int i = 0; i < std::get<0>(prob).size(); i++)
                std::get<0>(initial).push_back(i);
            return initial;
        };

        return KnapsackProblem;
    }



    std::tuple<std::vector<int>, std::vector<int>, int>
            GenerateRandomProblemConstants(std::pair<int,int> range, int size, double fitpercentage = 0.5)
    {
        // random engine
        std::random_device rd;
        std::mt19937 mt(1);
        std::uniform_int_distribution<int> dist(range.first, range.second);

        // knapsack consts
        std::vector<int> weights;
        std::vector<int> values;

        int sum = 0;
        for (int i=0; i<size; ++i){
           weights.push_back(dist(mt));
           values.push_back(dist(mt));
           sum += weights[i];
        }

        return {weights, values, (int)sum*fitpercentage};
    }

}


template<typename T>
int knapSack(T t) {};

// Returns the maximum value that
// can be put in a knapsack of capacity W
template<>
int knapSack(BnB::Knapsack::Consts consts)
{
    int W = std::get<2>(consts);
    std::vector<int> wt = std::get<0>(consts);
    std::vector<int> val = std::get<1>(consts);
    int n = val.size();

    int i, w;
    int K[n + 1][W + 1];

    // Build table K[][] in bottom up manner
    for (i = 0; i <= n; i++)
    {
        for (w = 0; w <= W; w++)
        {
            if (i == 0 || w == 0)
                K[i][w] = 0;
            else if (wt[i - 1] <= w)
                K[i][w] = std::max(val[i - 1]
                                   + K[i - 1][w - wt[i - 1]],
                                   K[i - 1][w]);
            else
                K[i][w] = K[i - 1][w];
        }
    }

    return K[n][W];
}



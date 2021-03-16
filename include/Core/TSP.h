#pragma once

#include "Base.h"

// factory function that returns a TSP problem definition
// used for testing solvers, and as an example of how to use the library

namespace BnB::TSP {
    using Consts = Problem_Constants<
            std::vector<int>, // connectivity Matrix
            int, // root node
            int>; // node count

    using Params = Subproblem_Parameters<
            std::vector<int>, // list of traveled nodes
            int // current value
    >;


    Problem_Definition<Consts, Params, int> GenerateToyProblem() {
        // object that will hold the functions called by the solver
        Problem_Definition<Consts, Params, int> TSPProblem;

        // you can assume that the subproblem is not feasible yet so it can be split
        TSPProblem.SplitSolution = [](const Consts &consts, const Params &params) {
            std::vector<Params> ret;
            int n = std::get<2>(consts);
            std::vector<int> connectivity = std::get<0>(consts);

            int CurrentNode = *(std::get<0>(params).end() - 1);

            for (int i = 0; i < n; i++) {
                int w = connectivity[n * CurrentNode + 1];
                if (w == -1)
                    continue;

                Params Subproblem = params;
                std::get<0>(Subproblem).push_back(i);
                std::get<1>(Subproblem) += w;
            }
            return ret;
        };

        TSPProblem.GetEstimateForBounds = [](const Consts &consts, const Params &params) {
            // calculate the cost as if all items were to fit in the sack
/*
            int ObjectIndex = std::get<0>(params).size();
            int sumCost = 1;
            for (int i = ObjectIndex; i < std::get<0>(consts).size(); i++)
                sumCost += std::get<1>(consts)[i];

    */
            return std::tuple<int, int>(1, 0);
        };

        TSPProblem.GetContainedUpperBound = [](const Consts &consts, const Params &params) {
            return std::get<1>(params);
        };

        TSPProblem.IsFeasible = [](const Consts &consts, const Params &params) {
            return FEASIBILITY::FULL;
        };


        TSPProblem.PrintSolution = [](const Params &params) {
            std::cout << "list of visited nodes is as follows " << std::endl;
            for (const auto &el : std::get<0>(params)) {
                std::cout << el << "->";
            }
            std::cout << std::endl;
        };

        TSPProblem.GetInitialSubproblem = [](const Consts &prob) {
            Params params;
            std::get<0>(params).push_back(std::get<1>(prob));
            return params;
        };

        return TSPProblem;
    }
}

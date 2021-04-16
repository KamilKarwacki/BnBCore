#pragma once

#include "Base.h"

// factory function that returns a LP problem definition
// used for testing solvers, and as an example of how to use the library

namespace BnB::IP{
    using Consts = Problem_Constants<
           std::vector<std::vector<double>>,// A
           std::vector<double>, //b
           std::vector<double>, //c
           double //v
           >;

    using Params = Subproblem_Parameters<
            std::vector<std::vector<double>>, // new conditions
            std::vector<double>, // variables after simplex solved
            int,  // variable that should be split next
            std::vector<double>, // b extension
            bool // feasible
    >;

    namespace Detail{
        auto SolveWithSimplex(const Consts& consts, const Params& params)
        {

            // build extended problem;
            std::vector<std::vector<double>> A_ext = std::get<0>(consts);
            std::vector<double>              b_ext = std::get<1>(consts);
            std::vector<double>                  c = std::get<2>(consts);
            double                               v = std::get<3>(consts);

            int extraConstraints = std::get<0>(params).size();
            for(int i = 0; i < extraConstraints; i++) {
                A_ext.push_back(std::get<0>(params)[i]);
                b_ext.push_back(std::get<3>(params)[i]);
            }




            return 123;
        }
/*
        auto Split(bool lower, const Consts& consts, const Params& params)
        {
            std::vector<std::vector<double>> NewConditions(std::get<0>(params)); // copy old conditions from the root
            std::vector<double> b_ext(std::get<3>(params));

            int ToBeSplit = std::get<2>(params);
            double Value  = std::get<1>(params)[ToBeSplit];

            std::vector<double> Condition(std::get<2>(consts).size());
            Condition[ToBeSplit] = lower ? 1 : -1;
            NewConditions.push_back(Condition);

            auto it = std::find(b_ext.begin(), b_ext.end(), lower ? std::floor(Value): -std::ceil(Value));
            if(it != b_ext.end())
            {
               int dist = std::distance(b_ext.begin(), it);
               // if the condition is already done
               if(NewConditions[dist][ToBeSplit] == Condition[ToBeSplit])
               {
                   // return infeasible solution
                   //Params BadParams;
                   //std::get<4>(BadParams);
                   //return BadParams;
               }
            }
            b_ext.push_back( lower ? std::floor(Value) : -std::ceil(Value));

            // copy all things from parent
            Params FirstCase = params;
            std::get<0>(FirstCase) = NewConditions;
            std::get<3>(FirstCase) = b_ext;

            // solve the new node
            auto RelaxedSolution = Detail::SolveWithSimplex(consts, FirstCase);
            std::get<4>(FirstCase) = RelaxedSolution.hasSolution() ==
            if(!std::get<4>(FirstCase)) return FirstCase;
            // find which point shall be split next
            int max = -1;
            int max_ind = -1;
            for(int i = 0; i < std::get<2>(consts).size(); i++)
            {
                double diff = abs(RelaxedSolution.getSolution()(i) - std::floor(RelaxedSolution.getSolution()(i)));
                if(diff > max){
                    max = diff;
                    max_ind = i;
                }
            }
            std::vector<double> SolutionVector;
            for(int i = 0; i < std::get<2>(consts).size(); i++)
                SolutionVector.push_back(RelaxedSolution.getSolution()(i));

            std::get<1>(FirstCase) = SolutionVector;
            std::get<2>(FirstCase) = max_ind;
            assert(max_ind != -1 and std::get<4>(FirstCase));
            return FirstCase;
        }*/
    }

    Problem_Definition<Consts, Params, double> GenerateToyProblem() {
        // object that will hold the functions called by the solver
        Problem_Definition<Consts, Params, double> LPProblem;
        /*


        // you can assume that the subproblem is not feasible yet so it can be split
        LPProblem.SplitSolution = [](const Consts &consts, const Params &params) {
            auto first = Detail::Split(true, consts, params);
            auto second = Detail::Split(false, consts, params);
            std::vector<Params> result;
            if(std::get<4>(first)) result.push_back(first);
            if(std::get<4>(second)) result.push_back(second);
            return result;
        };

        LPProblem.GetEstimateForBounds = [](const Consts &consts, const Params &params) {
            double Lower = 0;
            for(int i = 0; i < std::get<2>(consts).size(); i++)
                Lower += std::get<2>(consts)[i] * std::get<1>(params)[i];

            Lower += std::get<3>(consts);

            return std::tuple<double, double>(Lower, 0);
        };

        LPProblem.GetContainedUpperBound = [](const Consts &consts, const Params &params) {
            double Lower = 0;
            for(int i = 0; i < std::get<2>(consts).size(); i++)
                Lower += std::get<2>(consts)[i] * std::floor(std::get<1>(params)[i]);

            Lower += std::get<3>(consts);
            return Lower;
        };

        LPProblem.IsFeasible = [](const Consts &consts, const Params &params) {
            if(std::get<4>(params))
                return FEASIBILITY::Full;
            else return FEASIBILITY::NONE;
        };


        LPProblem.PrintSolution = [](const Params &params) {
            int s = std::get<1>(params).size();
            printf("Solution: (");
            for (int i=0;i<s;i++) printf("%lf%s", std::get<1>(params)[i], (i < s) ? ", " : ")\n");

        };

        LPProblem.GetInitialSubproblem = [](const Consts &consts) {
            Params params;

            auto RelaxedSolution = Detail::SolveWithSimplex(consts, params);
            std::get<4>(params) = RelaxedSolution.hasSolution() == simplex_lib::Solver<Eigen::MatrixXd>::SOL_FOUND;
            // find which point shall be split next
            int max = 0.0;
            int max_ind = -1;
            for(int i = 0; i < std::get<2>(consts).size(); i++)
            {
                double diff = abs(RelaxedSolution.getSolution().transpose()(i) - std::floor(RelaxedSolution.getSolution().transpose()(i)));
                if(diff > max){
                    max = diff;
                    max_ind = i;
                }
            }
            assert(max_ind != -1);

            std::vector<double> SolutionVector;
            for(int i = 0; i < std::get<2>(consts).size(); i++)
                SolutionVector.push_back(RelaxedSolution.getSolution().transpose()(i));

            std::get<1>(params) = SolutionVector;
            std::get<2>(params) = max_ind;

            return params;
        };
        */
        return LPProblem;
    }
}




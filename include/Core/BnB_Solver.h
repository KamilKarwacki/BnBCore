#pragma once

template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
class BnB_Solver {
public:
    // maximizes a problem defined by the user
    virtual Subproblem_Params
    Maximize(const Problem_Definition <Problem_Consts, Subproblem_Params, Domain_Type>&, const Problem_Consts &) = 0;

    // minimizes a problem defined by the user
    virtual Subproblem_Params
    Minimize(const Problem_Definition <Problem_Consts, Subproblem_Params, Domain_Type>& , const Problem_Consts &) = 0;
};

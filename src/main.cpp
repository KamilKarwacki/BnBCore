#include "BnB_Solver.h"


using myConst =  myLib::Problem_Constants<float, std::vector<int>>;
using myParams = myLib::Subproblem_Parameters<int, int>;

int main(int argc, char *argv[])
{
    Problem_Definition<myConst, myParams> Prob_Def;

    // another way to set up the functions we need
    // captureless lambdas can convert into functionpointers
    Prob_Def.GetInitialSubproblem = [](const myConst& c){
       std::cout << "hello im getting called" << std::endl;
       myParams p;
       std::get<0>(p) = 1;
       std::get<1>(p) = 10;
       return p;
    };

    myConst c;
    Solver<myConst, myParams> solver;
    solver.Maximize(Prob_Def, c);
}

#include "BnB_Solver.h"


using myConst = Problem_Constants<float, std::vector<int>>;
using myParams = Solution_Parameters<int, int>;

int main(int argc, char *argv[])
{
    Problem_Definition<myConst, myParams> Prob_Def;

    Prob_Def.GetInitialSolution = [](const myConst& c){
       std::cout << "hello im getting called" << std::endl;
       myParams p;
       std::get<0>(p) = 1;
       std::get<1>(p) = 10;
       return p;
    };

    myConst c;

    Solver<myConst, myParams> solver;
    solver.Solve(Prob_Def, c);
}

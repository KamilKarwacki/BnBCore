#pragma once

/*
 Petar 'PetarV' Velickovic
 Algorithm: Simplex Algorithm
*/

#include <iostream>
#include <vector>
#include <list>
#include <string>
#include <algorithm>
#include <queue>
#include <stack>
#include <set>
#include <map>
#include <complex>

typedef long long lld;
typedef unsigned long long llu;
using namespace std;

/*
 The Simplex algorithm aims to solve a linear program - optimising a linear function subject
 to linear constraints. As such it is useful for a very wide range of applications.

 N.B. The linear program has to be given in *slack form*, which is as follows:
 maximise
     c_1 * x_1 + c_2 * x_2 + ... + c_n * x_n + v
 subj. to
     a_11 * x_1 + a_12 * x_2 + ... + a_1n * x_n + b_1 = s_1
     a_21 * x_1 + a_22 * x_2 + ... + a_2n * x_n + b_2 = s_2
     ...
     a_m1 * x_1 + a_m2 * x_2 + ... + a_mn * x_n + b_m = s_m
 and
     x_1, x_2, ..., x_n, s_1, s_2, ..., s_m >= 0

 Every linear program can be translated into slack form; the parameters to specify are:
     - the number of variables, n, and the number of constraints, m;
     - the matrix A = [[A_11, A_12, ..., A_1n], ..., [A_m1, A_m2, ..., A_mn]];
     - the vector b = [b_1, b_2, ..., b_m];
     - the vector c = [c_1, c_2, ..., c_n] and the constant v.

 Complexity:    O(m^(n/2)) worst case
                O(n + m) average case (common)
*/

// pivot yth variable around xth constraint
inline void pivot(std::vector<std::vector<double>>& A, std::vector<double>& b,
                  std::vector<double>& c, std::vector<int>& N, std::vector<int>& B, double& v, int x, int y)
{
    //printf("Pivoting variable %d around constraint %d.\n", y, x);
    const int n = c.size();
    const int m = b.size();
    // first rearrange the x-th row
    for (int j=0;j<n;j++)
    {
        if (j != y)
        {
            A[x][j] /= -A[x][y];
        }
    }
    b[x] /= -A[x][y];
    A[x][y] = 1.0 / A[x][y];

    // now rearrange the other rows
    for (int i=0;i<m;i++)
    {
        if (i != x)
        {
            for (int j=0;j<n;j++)
            {
                if (j != y)
                {
                    A[i][j] += A[i][y] * A[x][j];
                }
            }
            b[i] += A[i][y] * b[x];
            A[i][y] *= A[x][y];
        }
    }

    // now rearrange the objective function
    for (int j=0;j<n;j++)
    {
        if (j != y)
        {
            c[j] += c[y] * A[x][j];
        }
    }
    v += c[y] * b[x];
    c[y] *= A[x][y];

    // finally, swap the basic & nonbasic variable
    swap(B[x], N[y]);
}

// Run a single iteration of the simplex algorithm.
// Returns: 0 if OK, 1 if STOP, -1 if UNBOUNDED
inline int iterate_simplex(std::vector<std::vector<double>>& A, std::vector<double>& b,
                           std::vector<double>& c, std::vector<int>& N, std::vector<int>& B, double& v)
{
    const int n = c.size();
    const int m = b.size();

    printf("--------------------\n");
    printf("State:\n");
    printf("Maximise: ");
    for (int j=0;j<n;j++) printf("%lfx_%d + ", c[j], N[j]);
    printf("%lf\n", v);
    printf("Subject to:\n");
    for (int i=0;i<m;i++)
    {
        for (int j=0;j<n;j++) printf("%lfx_%d + ", A[i][j], N[j]);
        printf("%lf = x_%d\n", b[i], B[i]);
    }


    // getchar(); // uncomment this for debugging purposes!

    int ind = -1, best_var = -1;
    for (int j=0;j<n;j++)
    {
        if (c[j] > 0)
        {
            if (best_var == -1 || N[j] < ind)
            {
                ind = N[j];
                best_var = j;
            }
        }
    }
    if (ind == -1) return 1;

    double max_constr = INFINITY;
    int best_constr = -1;
    for (int i=0;i<m;i++)
    {
        if (A[i][best_var] < 0)
        {
            double curr_constr = -b[i] / A[i][best_var];
            if (curr_constr < max_constr)
            {
                max_constr = curr_constr;
                best_constr = i;
            }
        }
    }
    if (isinf(max_constr)) return -1;
    else pivot(A,b,c,N,B,v,best_constr, best_var);

    return 0;
}

// (Possibly) converts the LP into a slack form with a feasible basic solution.
// Returns 0 if OK, -1 if INFEASIBLE
inline int initialise_simplex(std::vector<std::vector<double>>& A, std::vector<double>& b,
                              std::vector<double>& c, std::vector<int>& N, std::vector<int>& B, double& v)
{
    const int n = c.size();
    const int m = b.size();
    int k = -1;
    double min_b = -1;
    for (int i=0;i<m;i++)
    {
        if (k == -1 || b[i] < min_b)
        {
            k = i;
            min_b = b[i];
        }
    }

    if (b[k] >= 0) // basic solution feasible!
    {
        for (int j=0;j<n;j++) N[j] = j;
        for (int i=0;i<m;i++) B[i] = n + i;
        return 0;
    }

    // generate auxiliary LP
    int n_aux = n + 1;
    for (int j=0;j<n_aux;j++) N[j] = j;
    for (int i=0;i<m;i++) B[i] = n + i;

    // store the objective function
    double c_old[n];
    for (int j=0;j<n_aux-1;j++) c_old[j] = c[j];
    double v_old = v;

    // aux. objective function
    c[n_aux-1] = -1;
    for (int j=0;j<n_aux;j++) c[j] = 0;
    v = 0;
    // aux. coefficients
    for (int i=0;i<m;i++) A[i][n_aux - 1] = 1;

    // perform initial pivot
    pivot(A,b,c,N,B,v,k, n_aux - 1);

    // now solve aux. LP
    int code;
    while (!(code = iterate_simplex(A,b,c,N,B,v)));

    assert(code == 1); // aux. LP cannot be unbounded!!!

    if (v != 0) return -1; // infeasible!

    int z_basic = -1;
    for (int i=0;i<m;i++)
    {
        if (B[i] == n_aux)
        {
            z_basic = i;
            break;
        }
    }

    // if x_n basic, perform one degenerate pivot to make it nonbasic
    if (z_basic != -1) pivot(A,b,c,N,B,v,z_basic, n_aux - 1);

    int z_nonbasic = -1;
    for (int j=0;j<n_aux;j++)
    {
        if (N[j] == n_aux)
        {
            z_nonbasic = j;
            break;
        }
    }
    assert(z_nonbasic != -1);

    for (int i=0;i<m;i++)
    {
        A[i][z_nonbasic] = A[i][n_aux-1];
    }
    swap(N[z_nonbasic], N[n_aux - 1]);

    // USE N from before
    for (int j=0;j<n;j++) if (N[j] > n) N[j]--;
    for (int i=0;i<m;i++) if (B[i] > n) B[i]--;

    for (int j=0;j<n;j++) c[j] = 0;
    v = v_old;

    for (int j=0;j<n;j++)
    {
        bool ok = false;
        for (int jj=0;jj<n;jj++)
        {
            if (j == N[jj])
            {
                c[jj] += c_old[j];
                ok = true;
                break;
            }
        }
        if (ok) continue;
        for (int i=0;i<m;i++)
        {
            if (j == B[i])
            {
                for (int jj=0;jj<n;jj++)
                {
                    c[jj] += c_old[j] * A[i][jj];
                }
                v += c_old[j] * b[i];
                break;
            }
        }
    }

    return 0;
}

// Runs the simplex algorithm to optimise the LP.
// Returns a vector of -1s if unbounded, -2s if infeasible.
pair<vector<double>, double> simplex(std::vector<std::vector<double>>& A, std::vector<double>& b,
                                     std::vector<double>& c, double& v)
{
    using namespace std;
    const int n = c.size();
    const int m = b.size();
    std::vector<int> N(n+1), B(m);
    if (initialise_simplex(A, b, c, N, B, v) == -1)
    {
        return make_pair(vector<double>(n + m, -2), INFINITY);
    }

    int code;
    while (!(code = iterate_simplex(A, b, c, N, B, v)));

    if (code == -1) return make_pair(vector<double>(n + m, -1), INFINITY);

    vector<double> ret;
    ret.resize(n + m);
    for (int j=0;j<n;j++)
    {
        ret[N[j]] = 0;
    }
    for (int i=0;i<m;i++)
    {
        ret[B[i]] = b[i];
    }

    return make_pair(ret, v);
}

void initA(std::vector<std::vector<double>>& A)
{
    A[0][0] = 8000; A[0][1]=4000;
    A[1][0] = 15; A[1][1] = 30;

}

void initb(std::vector<double>& b)
{
    b[0] = 40000;
    b[1] = 200;
}

void initc(std::vector<double>& c)
{
    c[0]=100; c[1]=150;
}

void TestA()
{
    int n = 2, m = 4;
    std::vector<std::vector<double>> A;
    A.resize(m);
    for(int i = 0; i < m; i++)
        A[i].resize(m);
    A[0]= {-8000,-4000};
    A[1]= {-15,-30};
    A[2]= {0,1};
    A[3]={0,-1};


    std::vector<double> b;
    b = {40000,200, -3,2};
    std::vector<double> c;
    c = {100,150};

    double v = 0;
    pair<vector<double>, double> ret = simplex(A,b,c,v);

    if (isinf(ret.second))
    {
        if (ret.first[0] == -1) printf("Objective function unbounded!\n");
        else if (ret.first[0] == -2) printf("Linear program infeasible!\n");
    }else
    {
        printf("Solution: (");
        for (int i=0;i<n+m;i++) printf("%lf%s", ret.first[i], (i < n + m - 1) ? ", " : ")\n");
        printf("Optimal objective value: %lf\n", ret.second);
    }

}

int TestSimplex()
{

    int n = 2, m = 2;
    std::vector<std::vector<double>> A;
    A.resize(m);
    for(int i = 0; i < m; i++)
        A[i].resize(m);
    initA(A);

    std::vector<double> b(m);
    initb(b);
    std::vector<double> c(n);
    initc(c);

    double v = 0;
    pair<vector<double>, double> ret = simplex(A,b,c,v);

    if (isinf(ret.second))
    {
        if (ret.first[0] == -1) printf("Objective function unbounded!\n");
        else if (ret.first[0] == -2) printf("Linear program infeasible!\n");
    }
    else
    {
        printf("Solution: (");
        for (int i=0;i<n+m;i++) printf("%lf%s", ret.first[i], (i < n + m - 1) ? ", " : ")\n");
        printf("Optimal objective value: %lf\n", ret.second);
    }

    return 0;
}

/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#include "saiga/core/util/random.h"
#include "saiga/core/util/table.h"
#include "saiga/vision/Random.h"
#include "saiga/vision/recursiveMatrices/RecursiveMatrices.h"
#include "saiga/vision/recursiveMatrices/RecursiveSimplicialCholesky.h"
#include "saiga/vision/recursiveMatrices/SparseCholesky.h"

#include "Eigen/CholmodSupport"
#include "Eigen/SparseCholesky"

#include <fstream>


using namespace Saiga;



std::ofstream strm;

#define USE_BLOCKS

template <int block_size2, int factor>
class Sparse_LDLT_TEST
{
   public:
#ifdef USE_BLOCKS
    static constexpr int block_size = block_size2;
    using Block =
        typename std::conditional<block_size == 1, double, Eigen::Matrix<double, block_size, block_size>>::type;
    using Vector       = typename std::conditional<block_size == 1, double, Eigen::Matrix<double, block_size, 1>>::type;
    using WrappedBlock = typename std::conditional<block_size == 1, double, MatrixScalar<Block>>::type;
    using WrappedVector = typename std::conditional<block_size == 1, double, MatrixScalar<Vector>>::type;
    using AType         = Eigen::SparseMatrix<WrappedBlock>;
    using AType2        = Eigen::SparseMatrix<WrappedBlock, Eigen::RowMajor>;
    using VType         = Eigen::Matrix<WrappedVector, -1, 1>;
#else
    static constexpr int block_size = 1;
    using Block                     = double;
    using Vector                    = double;
    using AType                     = Eigen::SparseMatrix<Block>;
    using AType2                    = Eigen::SparseMatrix<Block, Eigen::RowMajor>;
    using VType                     = Eigen::Matrix<Vector, -1, 1>;
#endif

    //    const int n    = 128 * factor / block_size;
    //    const int nnzr = 4 * factor / block_size;

    const int n    = 5 * factor;
    const int nnzr = 2 * factor;

    Sparse_LDLT_TEST()
    {
        A.resize(n, n);
        A2.resize(n, n);
        Anoblock.resize(n * block_size, n * block_size);
        x.resize(n);
        b.resize(n);

        std::vector<Eigen::Triplet<Block>> trips;
        trips.reserve(nnzr * n * 2);

        for (int i = 0; i < n; ++i)
        {
            auto indices = Random::uniqueIndices(nnzr, n);
            std::sort(indices.begin(), indices.end());

            for (auto j : indices)
            {
                if (i != j)
                {
                    Block b = RecursiveRandom<Block>::get();
                    trips.emplace_back(i, j, b);
                    trips.emplace_back(j, i, transpose(b));
                }
            }

            // Make sure we have a symmetric diagonal block
            //            Vector dv = Random::sampleDouble(-1, 1);
            Vector dv = RecursiveRandom<Vector>::get();


#ifdef USE_BLOCKS
            Block D;
            if constexpr (block_size == 1)
                D = dv * dv;
            else
                D = dv * dv.transpose();
//            Block D = dv * dv.transpose();
#else
            Block D = dv * transpose(dv);
#endif

            // Make sure the matrix is positiv
            auto n = MultiplicativeNeutral<Block>::get();
            D += n * 5.0;
            //            D += 5;
            //            D.diagonal() += Vector::Ones() * 5;
            trips.emplace_back(i, i, D);

            b(i) = RecursiveRandom<Vector>::get();
        }
        A.setFromTriplets(trips.begin(), trips.end());
        A.makeCompressed();

        A2.setFromTriplets(trips.begin(), trips.end());
        A2.makeCompressed();


        std::vector<Eigen::Triplet<double>> trips_no_block;
        trips_no_block.reserve(nnzr * n * 2 * block_size * block_size);
        for (auto t : trips)
        {
            if constexpr (block_size == 1)
            {
                trips_no_block.emplace_back(t.row(), t.col(), t.value());
            }
            else
            {
                for (int i = 0; i < block_size; ++i)
                {
                    for (int j = 0; j < block_size; ++j)
                    {
                        trips_no_block.emplace_back(t.row() * block_size + i, t.col() * block_size + j,
                                                    t.value()(i, j));
                    }
                }
            }
        }
        Anoblock.setFromTriplets(trips_no_block.begin(), trips_no_block.end());
        Anoblock.makeCompressed();



        //        Random::setRandom(x);
        //        Random::setRandom(b);

        //        cout << expand(A) << endl << endl;
        //        cout << expand(Anoblock) << endl << endl;
        //        exit(0);
        //        cout << b.transpose() << endl;

        cout << "." << endl;
        cout << "Sparse LDLT test" << endl;
        cout << "Blocksize: " << block_size << endl;
        cout << "N: " << n << endl;
        cout << "Non zeros (per row): " << nnzr << endl;
        cout << "Non zeros: " << Anoblock.nonZeros() << endl;
        cout << "Density: " << (double)Anoblock.nonZeros() / double(n * n * block_size * block_size) << endl;
        cout << "." << endl;
        cout << endl;
    }

    auto solveEigenDenseLDLT()
    {
        x.setZero();
        auto Ae    = expand(A);
        auto be    = expand(b);
        auto bx    = expand(x);
        float time = 0;
        {
            Saiga::ScopedTimer<float> timer(time);
            bx = Ae.ldlt().solve(be);
        }
        double error = (Ae * bx - be).squaredNorm();
        return std::make_tuple(time, error, SAIGA_SHORT_FUNCTION);
    }

    auto solveEigenSparseLDLT()
    {
        x.setZero();
        auto be = expand(b);
        auto bx = expand(x);
        Eigen::SimplicialLDLT<decltype(Anoblock)> ldlt;

        float time = 0;
        {
            Saiga::ScopedTimer<float> timer(time);
            ldlt.compute(Anoblock);
            bx = ldlt.solve(be);
        }

        double error = (Anoblock * bx - be).squaredNorm();
        return std::make_tuple(time, error, SAIGA_SHORT_FUNCTION);
    }

    auto solveCholmod()
    {
        x.setZero();
        auto be = expand(b);
        auto bx = expand(x);
        Eigen::CholmodSimplicialLDLT<decltype(Anoblock)> ldlt;

        float time = 0;
        {
            Saiga::ScopedTimer<float> timer(time);
            //            Eigen::CholmodSupernodalLLT<decltype(Anoblock)> ldlt;
            ldlt.compute(Anoblock);
            bx = ldlt.solve(be);
        }
        double error = (Anoblock * bx - be).squaredNorm();
        return std::make_tuple(time, error, SAIGA_SHORT_FUNCTION);
    }


    auto solveEigenRecursiveSparseLDLT()
    {
        x.setZero();
        using LDLT = Eigen::RecursiveSimplicialLDLT<AType, Eigen::Lower>;

        float time = 0;
        {
            Saiga::ScopedTimer<float> timer(time);
            LDLT ldlt;
            ldlt.compute(A);
            //            ldlt.analyzePattern(A);
            //            ldlt.factorize(A);
            x = ldlt.solve(b);
        }

        //        cout << expand(x).transpose() << endl;
        double error = expand((A * x - b).eval()).squaredNorm();
        return std::make_tuple(time, error, SAIGA_SHORT_FUNCTION);
    }

    auto solveEigenRecursiveSparseLDLTRowMajor()
    {
        x.setZero();
        using LDLT = Eigen::RecursiveSimplicialLDLT<AType2, Eigen::Upper>;

        float time = 0;
        {
            Saiga::ScopedTimer<float> timer(time);
            LDLT ldlt;
            ldlt.compute(A2);
            //            ldlt.analyzePattern(A);
            //            ldlt.factorize(A);
            x = ldlt.solve(b);
        }

        //        cout << expand(x).transpose() << endl;
        double error = expand((A2 * x - b).eval()).squaredNorm();
        return std::make_tuple(time, error, SAIGA_SHORT_FUNCTION);
    }

    void solveSparseRecursive()
    {
        x.setZero();
        {
            SAIGA_BLOCK_TIMER();
            Saiga::SparseRecursiveLDLT<AType2, VType> ldlt;
            ldlt.compute(A2);
            x = ldlt.solve(b);
        }

        //        cout << expand(x).transpose() << endl;
        cout << "Error: " << expand((A * x - b).eval()).squaredNorm() << endl << endl;

        //        cout << "L" << endl << expand(ldlt.L) << endl << endl;
        //        cout << "D" << endl << expand(ldlt.D.toDenseMatrix()) << endl << endl;
    }
    Eigen::SparseMatrix<double> Anoblock;
    AType A;
    AType2 A2;
    VType x, b;
};

template <typename LDLT, typename T>
void make_test(LDLT& ldlt, Saiga::Table& tab, T f)
{
    std::vector<double> time;
    std::string name;
    float error;

    for (int i = 0; i < 10; ++i)
    {
        auto [time2, error2, name2] = (ldlt.*f)();
        time.push_back(time2);
        name  = name2;
        error = error2;
    }

    Saiga::Statistics s(time);
    float t = s.median;
    auto t2 = t / ldlt.Anoblock.nonZeros() * 1000;
    tab << name << t << t2 << error;

    strm << "," << t;
}

template <int block_size, int factor>
void run()
{
    using LDLT = Sparse_LDLT_TEST<block_size, factor>;
    LDLT test;
    strm << test.n << "," << test.Anoblock.nonZeros() << "," << block_size;

    Saiga::Table table{{35, 15, 15, 15}};
    table.setFloatPrecision(6);
    table << "Solver"
          << "Time (ms)"
          << "Time/NNZ (us)"
          << "Error";
    //    if (test.A.rows() < 100) make_test(test, table, &LDLT::solveEigenDenseLDLT);
    make_test(test, table, &LDLT::solveEigenSparseLDLT);
    make_test(test, table, &LDLT::solveEigenRecursiveSparseLDLT);
    make_test(test, table, &LDLT::solveEigenRecursiveSparseLDLTRowMajor);
    make_test(test, table, &LDLT::solveCholmod);

    strm << endl;
}



template <int START, int END, int factor>
struct LauncherLoop
{
    void operator()()
    {
        {
            run<START, factor>();
        }
        LauncherLoop<START + 1, END, factor> l;
        l();
    }
};

template <int END, int factor>
struct LauncherLoop<END, END, factor>
{
    void operator()() {}
};


int main(int, char**)
{
    Saiga::EigenHelper::checkEigenCompabitilty<2765>();
    Random::setSeed(15235);

    //    using LDLT = Sparse_LDLT_TEST<2, 1>;
    //    LDLT test;
    //    test.solveEigenRecursiveSparseLDLT();
    //    test.solveEigenRecursiveSparseLDLTRowMajor();

    strm.open("sparse_ldlt_benchmark.csv");
    strm << "n,nnz,block_size,eigen_sparse,eigen_recursive,cholmod" << endl;
    //        LauncherLoop<2, 8 + 1, 16> l;
    LauncherLoop<2, 4 + 1, 4> l;
    l();

    cout << "Done." << endl;
    return 0;
}

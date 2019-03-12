﻿/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#pragma once

#include "saiga/core/util/assert.h"
#include "saiga/vision/recursiveMatrices/MatrixScalar.h"

#include "saiga/core/util/random.h"

namespace Saiga
{


template <typename T>
struct RecursiveRandom
{
};


template <>
struct RecursiveRandom<double>
{
    static double get() { return Random::sampleDouble(-1,1); }
};

template <>
struct RecursiveRandom<float>
{
    static float get() { return Random::sampleDouble(-1,1); }
};



template <typename G>
struct RecursiveRandom<MatrixScalar<G>>
{
    static MatrixScalar<G> get() { return makeMatrixScalar(RecursiveRandom<G>::get()); }
};


template <typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
struct RecursiveRandom<Eigen::Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>>
{
    using Scalar = _Scalar;
    using ChildExpansion = RecursiveRandom<_Scalar>;
    using MatrixType = Eigen::Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>;

    static MatrixType get()
    {
        MatrixType A;

        for (int i = 0; i < A.rows(); ++i)
        {
            for (int j = 0; j < A.cols(); ++j)
            {
                A(i,j) = ChildExpansion::get();
            }
        }
        return A;
    }
};

}  // namespace Saiga
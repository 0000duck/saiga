﻿/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */
#include "BARecursive.h"

#include "saiga/core/imgui/imgui.h"
#include "saiga/core/time/timer.h"
#include "saiga/core/util/Algorithm.h"
#include "saiga/core/util/Thread/omp.h"
#include "saiga/vision/util/HistogramImage.h"
#include "saiga/vision/util/LM.h"
#include "saiga/vision/kernels/BAPose.h"
#include "saiga/vision/kernels/BAPosePoint.h"
#include "saiga/vision/kernels/Robust.h"

#include <fstream>
#include <numeric>

#define RECURSIVE_BA_USE_TIMERS false

namespace Saiga
{
void BARec::reserve(int n, int m)
{
    validImages.reserve(n);
    validPoints.reserve(m);
    pointToValidMap.resize(m);
    cameraPointCounts.reserve(n);
    cameraPointCountsScan.reserve(n);
    pointCameraCounts.reserve(m);
    pointCameraCountsScan.reserve(m);

    pointDiagTemp.reserve(m);
    pointResTemp.reserve(m);

    localChi2.reserve(64);

    x_u.reserve(n);
    oldx_u.reserve(n);
    x_v.reserve(n);
    oldx_v.reserve(n);
}

void BARec::init()
{
    SAIGA_ASSERT(1 == OMP::getNumThreads());
    //    threads = 4;
    //    std::cout << "Test sizes: " << sizeof(Scene) << " " << sizeof(BARec)<< " " << sizeof(BABase)<< " " <<
    //    sizeof(LMOptimizer) << std::endl; std::cout << "Test sizes2: " << sizeof(BAMatrix) << " " <<
    //    sizeof(BAVector)<< " " << sizeof(BASolver)<< " " << sizeof(AlignedVector<SE3>) << std::endl;


    //    threads      = OMP::getNumThreads();
    Scene& scene = *_scene;

    SAIGA_OPTIONAL_BLOCK_TIMER(RECURSIVE_BA_USE_TIMERS && optimizationOptions.debugOutput);


    // currently the scene must be in a valid state
    SAIGA_ASSERT(scene);

    if (optimizationOptions.solverType == OptimizationOptions::SolverType::Direct)
    {
        explizitSchur = true;
        computeWT     = true;
    }
    else
    {
        explizitSchur = false;
        computeWT     = true;
    }



    // Check how many valid and cameras exist and construct the compact index sets
    validPoints.clear();
    validImages.clear();
    pointToValidMap.clear();
    validImages.reserve(scene.images.size());
    validPoints.reserve(scene.worldPoints.size());
    pointToValidMap.resize(scene.worldPoints.size());

    for (int i = 0; i < (int)scene.images.size(); ++i)
    {
        auto& img = scene.images[i];
        if (!img) continue;
        validImages.push_back(i);
    }

    for (int i = 0; i < (int)scene.worldPoints.size(); ++i)
    {
        auto& wp = scene.worldPoints[i];
        if (!wp) continue;
        int validId        = validPoints.size();
        pointToValidMap[i] = validId;
        validPoints.push_back(i);
    }

    n = validImages.size();
    m = validPoints.size();


    SAIGA_ASSERT(n > 0 && m > 0);

    A.resize(n, m);
    //    U.resize(n);
    //    V.resize(m);

    delta_x.resize(n, m);
    b.resize(n, m);

    x_u.resize(n);
    oldx_u.resize(n);
    x_v.resize(m);
    oldx_v.resize(m);


    // Make a copy of the initial parameters
    for (int i = 0; i < (int)validImages.size(); ++i)
    {
        auto& img = scene.images[validImages[i]];
        x_u[i]    = scene.extrinsics[img.extr].se3;
    }

    for (int i = 0; i < (int)validPoints.size(); ++i)
    {
        auto& wp = scene.worldPoints[validPoints[i]];
        x_v[i]   = wp.p;
    }

    cameraPointCounts.clear();
    cameraPointCounts.resize(n, 0);
    cameraPointCountsScan.resize(n);
    pointCameraCounts.clear();
    pointCameraCounts.resize(m, 0);
    pointCameraCountsScan.resize(m);
    observations = 0;

    std::vector<int> innerElements;
    for (int i = 0; i < (int)validImages.size(); ++i)
    {
        auto& img = scene.images[validImages[i]];
        for (auto& ip : img.stereoPoints)
        {
            if (ip.wp == -1) continue;

            int j = pointToValidMap[ip.wp];
            cameraPointCounts[i]++;
            pointCameraCounts[j]++;
            innerElements.push_back(j);
            observations++;
        }
    }

    auto test1 =
        Saiga::exclusive_scan(cameraPointCounts.begin(), cameraPointCounts.end(), cameraPointCountsScan.begin(), 0);
    auto test2 =
        Saiga::exclusive_scan(pointCameraCounts.begin(), pointCameraCounts.end(), pointCameraCountsScan.begin(), 0);

    SAIGA_ASSERT(test1 == observations && test2 == observations);

    // preset the outer matrix structure
    //    W.resize(n, m);
    A.w.setZero();
    A.w.reserve(observations);

    for (int k = 0; k < A.w.outerSize(); ++k)
    {
        A.w.outerIndexPtr()[k] = cameraPointCountsScan[k];
    }
    A.w.outerIndexPtr()[A.w.outerSize()] = observations;


    for (int i = 0; i < observations; ++i)
    {
        A.w.innerIndexPtr()[i] = innerElements[i];
    }

    // ===== Threading Tmps ======
    localChi2.resize(threads);
    pointDiagTemp.resize(threads - 1);
    pointResTemp.resize(threads - 1);
    for (auto& a : pointDiagTemp) a.resize(m);
    for (auto& a : pointResTemp) a.resize(m);



    // Setup the linear solver and anlyze the pattern
    loptions.maxIterativeIterations = optimizationOptions.maxIterativeIterations;
    loptions.iterativeTolerance     = optimizationOptions.iterativeTolerance;
    loptions.solverType             = (optimizationOptions.solverType == OptimizationOptions::SolverType::Direct)
                              ? Eigen::Recursive::LinearSolverOptions::SolverType::Direct
                              : Eigen::Recursive::LinearSolverOptions::SolverType::Iterative;
    loptions.buildExplizitSchur = explizitSchur;
    solver.analyzePattern(A, loptions);


    // Create sparsity histogram of the schur complement
    if (optimizationOptions.debugOutput)
    {
        HistogramImage img(n, n, 512, 512);
        schurStructure.clear();
        schurStructure.resize(n, std::vector<int>(n, -1));
        for (auto& wp : scene.worldPoints)
        {
            for (auto& ref : wp.stereoreferences)
            {
                for (auto& ref2 : wp.stereoreferences)
                {
                    int i1 = validImages[ref.first];
                    int i2 = validImages[ref2.first];

                    schurStructure[i1][ref2.first] = ref2.first;
                    schurStructure[i2][ref.first]  = ref.first;
                    img.add(i1, ref2.first, 1);
                    img.add(i2, ref.first, 1);
                }
            }
        }

        // compact it
        schurEdges = 0;
        for (auto& v : schurStructure)
        {
            v.erase(std::remove(v.begin(), v.end(), -1), v.end());
            schurEdges += v.size();
        }
        //        img.writeBinary("vision_ba_schur_sparsity.png");
    }


    // Create a sparsity histogram of the complete matrix
    if (false && optimizationOptions.debugOutput)
    {
        HistogramImage img(n + m, n + m, 512, 512);
        for (int i = 0; i < n + m; ++i)
        {
            img.add(i, i, 1);
        }

        for (int i = 0; i < (int)validImages.size(); ++i)
        {
            auto& img2 = scene.images[validImages[i]];
            for (auto& ip : img2.stereoPoints)
            {
                if (!ip) continue;

                int j   = pointToValidMap[ip.wp];
                int iid = validImages[i];
                img.add(iid, j + n, 1);
                img.add(j + n, iid, 1);
            }
        }
        img.writeBinary("vision_ba_complete_sparsity.png");
    }


    if (optimizationOptions.debugOutput)
    {
        std::cout << "." << std::endl;
        std::cout << "Structure Analyzed." << std::endl;
        std::cout << "Cameras: " << n << std::endl;
        std::cout << "Points: " << m << std::endl;
        std::cout << "Observations: " << observations << std::endl;
#if 1
        std::cout << "Schur Edges: " << schurEdges << std::endl;
        std::cout << "Non Zeros LSE: " << schurEdges * 6 * 6 << std::endl;
        std::cout << "Density: " << double(schurEdges * 6.0 * 6) / double(double(n) * n * 6 * 6) * 100 << "%"
                  << std::endl;
#endif

#if 1
        // extended analysis
        double averageCams   = 0;
        double averagePoints = 0;
        for (auto i : cameraPointCounts) averagePoints += i;
        averagePoints /= cameraPointCounts.size();
        for (auto i : pointCameraCounts) averageCams += i;
        averageCams /= pointCameraCounts.size();
        std::cout << "Average Points per Camera: " << averagePoints << std::endl;
        std::cout << "Average Cameras per Point: " << averageCams << std::endl;
#endif
        std::cout << "." << std::endl;
    }
}

double BARec::computeQuadraticForm()
{
    Scene& scene = *_scene;
    SAIGA_ASSERT(threads == OMP::getNumThreads());

    //    SAIGA_OPTIONAL_BLOCK_TIMER(RECURSIVE_BA_USE_TIMERS && optimizationOptions.debugOutput);

    using T = BlockBAScalar;
    //    using KernelType = Saiga::Kernel::BAPosePointMono<T>;


    //    b.u.setZero();
    //    b.v.setZero();
    //    A.u.setZero();
    //    A.v.setZero();



    SAIGA_ASSERT(A.w.IsRowMajor);



    //#pragma omp parallel num_threads(threads)
    {
        int tid = OMP::getThreadNum();


        double& newChi2 = localChi2[tid];
        newChi2         = 0;
        BDiag* bdiagArray;
        BRes* bresArray;

        if (tid == 0)
        {
            // thread 0 directly writes into the recursive matrix
            bdiagArray = &A.v.diagonal()(0).get();
            bresArray  = &b.v(0).get();
        }
        else
        {
            bdiagArray = pointDiagTemp[tid - 1].data();
            bresArray  = pointResTemp[tid - 1].data();
        }

        // every thread has to zero its own local copy
        for (int i = 0; i < m; ++i)
        {
            bdiagArray[i].setZero();
            bresArray[i].setZero();
        }

#pragma omp for
        for (int i = 0; i < (int)validImages.size(); ++i)
        {
            int k = A.w.outerIndexPtr()[i];
            //            SAIGA_ASSERT(k == A.w.outerIndexPtr()[i]);

            int imgid = validImages[i];
            auto& img = scene.images[imgid];
            //            auto& extr   = scene.extrinsics[img.extr].se3;
            auto& extr   = x_u[i];
            auto& extr2  = scene.extrinsics[img.extr];
            auto& camera = scene.intrinsics[img.intr];
            StereoCamera4 scam(camera, scene.bf);

            // each thread can direclty right into A.u and b.u because
            // we parallize over images
            auto& targetPosePose = A.u.diagonal()(i).get();
            auto& targetPoseRes  = b.u(i).get();
            targetPosePose.setZero();
            targetPoseRes.setZero();

            for (auto& ip : img.stereoPoints)
            {
                if (ip.wp == -1) continue;
                if (ip.outlier)
                {
                    A.w.valuePtr()[k].get().setZero();
                    ++k;
                    continue;
                }
                BlockBAScalar w = ip.weight * img.imageWeight * scene.scale();
                int j           = pointToValidMap[ip.wp];


                //                auto& wp        = scene.worldPoints[ip.wp].p;
                auto& wp = x_v[j];

                //                WElem targetPosePoint;
                WElem& targetPosePoint = A.w.valuePtr()[k].get();

                //                BDiag& targetPointPoint = A.v.diagonal()(j).get();
                //                BRes& targetPointRes    = b.v(j).get();
                BDiag& targetPointPoint = bdiagArray[j];
                BRes& targetPointRes    = bresArray[j];

                if (ip.depth > 0)
                {
                    using KernelType = Saiga::Kernel::BAPosePointStereo<T>;
                    KernelType::PoseJacobiType JrowPose;
                    KernelType::PointJacobiType JrowPoint;
                    KernelType::ResidualType res;

                    KernelType::evaluateResidualAndJacobian(scam, extr, wp, ip.point, ip.depth, 1, res, JrowPose,
                                                            JrowPoint);
                    if (extr2.constant) JrowPose.setZero();


                    auto c      = res.squaredNorm();
                    auto sqrtrw = sqrt(w);
                    if (baOptions.huberStereo > 0)
                    {
                        auto rw = Kernel::huberWeight<T>(baOptions.huberStereo, c);
                        sqrtrw *= sqrt(rw(1));
                    }
                    JrowPose *= sqrtrw;
                    JrowPoint *= sqrtrw;
                    res *= sqrtrw;

                    newChi2 += res.squaredNorm();

                    targetPosePose += JrowPose.transpose() * JrowPose;
                    targetPointPoint += JrowPoint.transpose() * JrowPoint;
                    targetPosePoint = JrowPose.transpose() * JrowPoint;
                    targetPoseRes -= JrowPose.transpose() * res;
                    targetPointRes -= JrowPoint.transpose() * res;
                }
                else
                {
                    using KernelType = Saiga::Kernel::BAPosePointMono<T>;
                    KernelType::PoseJacobiType JrowPose;
                    KernelType::PointJacobiType JrowPoint;
                    KernelType::ResidualType res;

                    KernelType::evaluateResidualAndJacobian(camera, extr, wp, ip.point, 1, res, JrowPose, JrowPoint);
                    if (extr2.constant) JrowPose.setZero();

                    auto c      = res.squaredNorm();
                    auto sqrtrw = sqrt(w);
                    if (baOptions.huberMono > 0)
                    {
                        auto rw = Kernel::huberWeight<T>(baOptions.huberMono, c);
                        sqrtrw *= sqrt(rw(1));
                    }
                    JrowPose *= sqrtrw;
                    JrowPoint *= sqrtrw;
                    res *= sqrtrw;

                    newChi2 += res.squaredNorm();

                    targetPosePose += JrowPose.transpose() * JrowPose;
                    targetPointPoint += JrowPoint.transpose() * JrowPoint;
                    targetPosePoint = JrowPose.transpose() * JrowPoint;
                    targetPoseRes -= JrowPose.transpose() * res;
                    targetPointRes -= JrowPoint.transpose() * res;
                }

                SAIGA_ASSERT(A.w.innerIndexPtr()[k] == j);
                //                A.w.innerIndexPtr()[k] = j;
                //                A.w.valuePtr()[k]      = targetPosePoint;

                ++k;
            }
        }

#pragma omp for
        for (int i = 0; i < m; ++i)
        {
            for (int j = 0; j < threads - 1; ++j)
            {
                A.v.diagonal()(i).get() += pointDiagTemp[j][i];
                b.v(i).get() += pointResTemp[j][i];
            }
        }
    }


    double chi2 = 0;
    for (int i = 0; i < threads; ++i)
    {
        chi2 += localChi2[i];
    }

    return chi2;
}

void BARec::addDelta()
{
    //#pragma omp parallel num_threads(threads)
    {
#pragma omp for nowait
        for (int i = 0; i < n; ++i)
        {
            oldx_u[i] = x_u[i];
            auto t    = delta_x.u(i).get();
            x_u[i]    = SE3::exp(t) * x_u[i];
        }
#pragma omp for nowait
        for (int i = 0; i < m; ++i)
        {
            oldx_v[i] = x_v[i];
            auto t    = delta_x.v(i).get();
            x_v[i] += t;
        }
    }
}

void BARec::revertDelta()
{
    //#pragma omp parallel num_threads(threads)
    {
#pragma omp for nowait
        for (int i = 0; i < n; ++i)
        {
            x_u[i] = oldx_u[i];
        }
#pragma omp for nowait
        for (int i = 0; i < m; ++i)
        {
            x_v[i] = oldx_v[i];
        }
    }
    //    x_u = oldx_u;
    //    x_v = oldx_v;
}
void BARec::finalize()
{
    Scene& scene = *_scene;

    SAIGA_OPTIONAL_BLOCK_TIMER(RECURSIVE_BA_USE_TIMERS && optimizationOptions.debugOutput);

    //#pragma omp parallel num_threads(threads)
    {
#pragma omp for nowait
        for (size_t i = 0; i < validImages.size(); ++i)
        {
            auto id    = validImages[i];
            auto& extr = scene.extrinsics[id];
            if (!extr.constant) extr.se3 = x_u[i];
        }
#pragma omp for
        for (size_t i = 0; i < validPoints.size(); ++i)
        {
            Eigen::Matrix<BlockBAScalar, 3, 1> t;
            auto id = validPoints[i];
            auto& p = scene.worldPoints[id].p;
            p       = x_v[i];
        }
    }
}


void BARec::addLambda(double lambda)
{
    //#pragma omp parallel num_threads(threads)
    {
        applyLMDiagonal(A.u, lambda);
        applyLMDiagonal(A.v, lambda);
    }
}



void BARec::solveLinearSystem()
{
    SAIGA_OPTIONAL_BLOCK_TIMER(RECURSIVE_BA_USE_TIMERS && optimizationOptions.debugOutput);
    //#pragma omp parallel num_threads(threads)
    {
        solver.solve(A, delta_x, b, loptions);
    }
}

double BARec::computeCost()
{
    Scene& scene = *_scene;

    SAIGA_OPTIONAL_BLOCK_TIMER(RECURSIVE_BA_USE_TIMERS && optimizationOptions.debugOutput);

    using T = BlockBAScalar;

    //#pragma omp parallel num_threads(threads)
    {
        int tid = OMP::getThreadNum();

        double& newChi2 = localChi2[tid];
        newChi2         = 0;
#pragma omp for
        for (int i = 0; i < (int)validImages.size(); ++i)
        {
            int imgid = validImages[i];
            auto& img = scene.images[imgid];
            //            auto& extr   = scene.extrinsics[img.extr].se3;
            auto& extr   = x_u[i];
            auto& camera = scene.intrinsics[img.intr];
            StereoCamera4 scam(camera, scene.bf);

            for (auto& ip : img.stereoPoints)
            {
                if (!ip) continue;
                BlockBAScalar w = ip.weight * img.imageWeight * scene.scale();
                int j           = pointToValidMap[ip.wp];
                auto& wp        = x_v[j];

                if (ip.depth > 0)
                {
                    using KernelType = Saiga::Kernel::BAPosePointStereo<T>;
                    KernelType::ResidualType res;
                    res         = KernelType::evaluateResidual(scam, extr, wp, ip.point, ip.depth, 1);
                    auto c      = res.squaredNorm();
                    auto sqrtrw = sqrt(w);
                    if (baOptions.huberStereo > 0)
                    {
                        auto rw = Kernel::huberWeight<T>(baOptions.huberStereo, c);
                        sqrtrw *= sqrt(rw(1));
                    }
                    res *= sqrtrw;
                    newChi2 += res.squaredNorm();
                }
                else
                {
                    using KernelType = Saiga::Kernel::BAPosePointMono<T>;
                    KernelType::ResidualType res;

                    res         = KernelType::evaluateResidual(camera, extr, wp, ip.point, 1);
                    auto c      = res.squaredNorm();
                    auto sqrtrw = sqrt(w);
                    if (baOptions.huberMono > 0)
                    {
                        auto rw = Kernel::huberWeight<T>(baOptions.huberMono, c);
                        sqrtrw *= sqrt(rw(1));
                    }
                    res *= sqrtrw;
                    newChi2 += res.squaredNorm();
                }
            }
        }
    }

    double chi2 = 0;
    for (int i = 0; i < threads; ++i)
    {
        chi2 += localChi2[i];
    }

    return chi2;
}
}  // namespace Saiga
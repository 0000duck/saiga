﻿#include "BAPoseOnly.h"

#include "saiga/time/timer.h"
#include "saiga/util/random.h"
#include "saiga/vision/BlockDiagonalMatrix.h"
#include "saiga/vision/BlockRecursiveBATemplates.h"
#include "saiga/vision/BlockSparseMatrix.h"
#include "saiga/vision/BlockVector.h"
#include "saiga/vision/Eigen_Compile_Checker.h"
#include "saiga/vision/MatrixScalar.h"
#include "saiga/vision/SparseHelper.h"
#include "saiga/vision/VisionIncludes.h"
#include "saiga/vision/kernels/BAPose.h"
#include "saiga/vision/kernels/BAPosePoint.h"

#include "Eigen/Sparse"
#include "Eigen/SparseCholesky"

#include <fstream>

namespace Saiga
{
void BAPoseOnly::poseOnlySparse(Scene& scene, int its)
{
    SAIGA_BLOCK_TIMER();

    using T          = double;
    using KernelType = Saiga::Kernel::BAPoseMono<T>;
    KernelType::PoseJacobiType JrowPose;
    KernelType::ResidualType res;

    auto numCameras = scene.extrinsics.size();

    std::vector<KernelType::PoseDiaBlockType> diagBlocks(numCameras);
    std::vector<KernelType::ResidualBlockType> resBlocks(numCameras);

    for (int k = 0; k < its; ++k)
    {
        for (size_t i = 0; i < numCameras; ++i)
        {
            diagBlocks[i].setZero();
            resBlocks[i].setZero();
        }

        for (auto& img : scene.images)
        {
            auto extr   = scene.extrinsics[img.extr].se3;
            auto camera = scene.intrinsics[img.intr];

            for (auto& ip : img.monoPoints)
            {
                if (!ip) continue;
                auto wp = scene.worldPoints[ip.wp].p;
                Saiga::Kernel::BAPoseMono<T>::evaluateResidualAndJacobian(camera, extr, wp, ip.point, res, JrowPose,
                                                                          ip.weight);
                diagBlocks[img.extr] += (JrowPose.transpose() * JrowPose).template triangularView<Eigen::Upper>();
                resBlocks[img.extr] += JrowPose.transpose() * res;
            }
        }

        for (size_t i = 0; i < numCameras; ++i)
        {
            Sophus::SE3d::Tangent t = diagBlocks[i].selfadjointView<Eigen::Upper>().ldlt().solve(resBlocks[i]);
            auto& se3               = scene.extrinsics[i].se3;
            se3                     = Sophus::SE3d::exp(t) * se3;
        }
    }
}


void BAPoseOnly::posePointDense(Scene& scene, int its)
{
    using T          = double;
    using KernelType = Saiga::Kernel::BAPosePointMono<T>;
    KernelType::PoseJacobiType JrowPose;
    KernelType::PointJacobiType JrowPoint;
    KernelType::ResidualType res;

    SAIGA_BLOCK_TIMER();
    auto numCameras  = scene.extrinsics.size();
    auto numPoints   = scene.worldPoints.size();
    auto numUnknowns = numCameras * 6 + numPoints * 3;
    using MatrixType = Eigen::MatrixXd;
    MatrixType JtJ(numUnknowns, numUnknowns);
    Eigen::VectorXd Jtb(numUnknowns);



    for (int k = 0; k < its; ++k)
    {
        JtJ.setZero();
        Jtb.setZero();

        for (auto& img : scene.images)
        {
            auto extr   = scene.extrinsics[img.extr].se3;
            auto camera = scene.intrinsics[img.intr];

            for (auto& ip : img.monoPoints)
            {
                if (!ip)
                {
                    SAIGA_ASSERT(0);
                    continue;
                }

                auto wp = scene.worldPoints[ip.wp].p;



                KernelType::evaluateResidualAndJacobian(camera, extr, wp, ip.point, ip.weight, res, JrowPose,
                                                        JrowPoint);

#if 0
                auto poseStart  = img.extr * 6;
                auto pointStart = numCameras * 6 + ip.wp * 3;
                JtJ.block(poseStart, poseStart, 6, 6) +=
                    (JrowPose.transpose() * JrowPose);  //.template triangularView<Eigen::Upper>();

                JtJ.block(pointStart, pointStart, 3, 3) +=
                    (JrowPoint.transpose() * JrowPoint);  //.template triangularView<Eigen::Upper>();

                JtJ.block(poseStart, pointStart, 6, 3) = JrowPose.transpose() * JrowPoint;
                JtJ.block(pointStart, poseStart, 3, 6) = JrowPoint.transpose() * JrowPose;

                Jtb.segment(poseStart, 6) += JrowPose.transpose() * res;
                Jtb.segment(pointStart, 3) += JrowPoint.transpose() * res;
#else
                auto pointStart = ip.wp * 3;
                auto poseStart  = numPoints * 3 + img.extr * 6;
                JtJ.block(pointStart, pointStart, 3, 3) += (JrowPoint.transpose() * JrowPoint);
                JtJ.block(poseStart, poseStart, 6, 6) += (JrowPose.transpose() * JrowPose);


                JtJ.block(poseStart, pointStart, 6, 3) = JrowPose.transpose() * JrowPoint;
                JtJ.block(pointStart, poseStart, 3, 6) = JrowPoint.transpose() * JrowPose;

                Jtb.segment(pointStart, 3) += JrowPoint.transpose() * res;
                Jtb.segment(poseStart, 6) += JrowPose.transpose() * res;
#endif
            }
        }

        //        cout << JtJ << endl << endl;


        //        std::ofstream strm("jtjdense.txt");
        //        strm << JtJ << endl;
        //        strm.close();


        if (0)
        {
            double lambda = 1;
            // lm diagonal
            for (int i = 0; i < numUnknowns; ++i)
            {
                // that's what g2o does
                JtJ(i, i) += lambda;  // * JtJ(i, i);
            }
        }


        Eigen::VectorXd x = JtJ.ldlt().solve(Jtb);


        Eigen::VectorXd x1 = x.segment(0, numPoints * 3);
        Eigen::VectorXd x2 = x.segment(numPoints * 3, numCameras * 6);

        cout << x1.transpose() << endl;
        cout << x2.transpose() << endl;
        return;

        for (size_t i = 0; i < numPoints; ++i)
        {
            Vec3 t  = x1.segment(i * 3, 3);
            auto& p = scene.worldPoints[i].p;
            p += t;
        }

        for (size_t i = 0; i < numCameras; ++i)
        {
            Sophus::SE3d::Tangent t = x2.segment(i * 6, 6);
            auto& se3               = scene.extrinsics[i].se3;
            se3                     = Sophus::SE3d::exp(t) * se3;
        }
    }
}


void BAPoseOnly::posePointDenseBlock(Scene& scene, int its)
{
    // test schur

    if (0)
    {
        Eigen::Matrix4d M = Eigen::Matrix4d::Random();
        Eigen::Vector4d b = Eigen::Vector4d::Random();
        Eigen::Vector4d x = M.lu().solve(b);
        cout << "x " << x.transpose() << endl;

        using Mat2 = Eigen::Matrix2d;
        using Vec2 = Eigen::Vector2d;

        Mat2 A = M.block(0, 0, 2, 2);
        Mat2 B = M.block(0, 2, 2, 2);
        Mat2 C = M.block(2, 0, 2, 2);
        Mat2 D = M.block(2, 2, 2, 2);

        Vec2 b1 = b.segment(0, 2);
        Vec2 b2 = b.segment(2, 2);


        Mat2 L  = C * A.inverse() * B - D;
        Vec2 r  = C * A.inverse() * b1 - b2;
        auto x2 = L.lu().solve(r);
        Vec2 r2 = b1 - B * x2;
        auto x1 = A.lu().solve(r2);

        x.segment(0, 2) = x1;
        x.segment(2, 2) = x2;

        cout << "x " << x.transpose() << endl;


        //        cout << M << endl << endl;


        exit(0);
    }
    posePointDense(scene, 1);
    cout << endl;

    using T          = double;
    using KernelType = Saiga::Kernel::BAPosePointMono<T>;
    KernelType::PoseJacobiType JrowPose;
    KernelType::PointJacobiType JrowPoint;
    KernelType::ResidualType res;

    SAIGA_BLOCK_TIMER();
    auto numCameras  = scene.extrinsics.size();
    auto numPoints   = scene.worldPoints.size();
    auto numUnknowns = numCameras * 6 + numPoints * 3;
    using MatrixType = Eigen::MatrixXd;


    auto n = numPoints * 3;
    auto m = numCameras * 6;

    MatrixType A(n, n);
    MatrixType B(n, m);
    MatrixType D(m, m);

    Eigen::VectorXd b1(n);
    Eigen::VectorXd b2(m);

    BlockDiagonalMatrix<KernelType::PointDiaBlockType> Ad(numPoints);
    BlockDiagonalMatrix<KernelType::PoseDiaBlockType> Dd(numCameras);

    for (int k = 0; k < its; ++k)
    {
        A.setZero();
        B.setZero();
        D.setZero();
        b1.setZero();
        b2.setZero();

        Ad.setZero();
        Dd.setZero();

        for (auto& img : scene.images)
        {
            auto extr   = scene.extrinsics[img.extr].se3;
            auto camera = scene.intrinsics[img.intr];

            for (auto& ip : img.monoPoints)
            {
                if (!ip)
                {
                    SAIGA_ASSERT(0);
                    continue;
                }

                auto wp = scene.worldPoints[ip.wp].p;

                auto pointStart = ip.wp * 3;
                auto poseStart  = img.extr * 6;


                KernelType::evaluateResidualAndJacobian(camera, extr, wp, ip.point, ip.weight, res, JrowPose,
                                                        JrowPoint);

                //                A.block(pointStart, pointStart, 3, 3) += (JrowPoint.transpose() * JrowPoint);
                //                D.block(poseStart, poseStart, 6, 6) += (JrowPose.transpose() * JrowPose);


                Ad(ip.wp)    = (JrowPoint.transpose() * JrowPoint);
                Dd(img.extr) = (JrowPose.transpose() * JrowPose);

                B.block(pointStart, poseStart, 3, 6) = JrowPoint.transpose() * JrowPose;

                b1.segment(pointStart, 3) += JrowPoint.transpose() * res;
                b2.segment(poseStart, 6) += JrowPose.transpose() * res;
            }
        }

        double lambda = 1;
        Ad.addToDiagonal(lambda * Eigen::VectorXd::Ones(n));
        Dd.addToDiagonal(lambda * Eigen::VectorXd::Ones(m));

        A = Ad.dense();
        D = Dd.dense();


        Eigen::VectorXd x1(n);
        Eigen::VectorXd x2(m);
        x1.setZero();
        x2.setZero();
#if 1
        // Schur complement solution

        auto C = B.transpose();

        Eigen::MatrixXd L  = C * Ad.inverse() * B - D;
        Eigen::VectorXd r  = C * Ad.inverse() * b1 - b2;
        x2                 = L.ldlt().solve(r);
        Eigen::VectorXd r2 = b1 - B * x2;
        x1                 = A.ldlt().solve(r2);

        //        cout << x1.transpose() << endl;
        //        cout << x2.transpose() << endl;

#else
        Eigen::MatrixXd JtJ(n + m, n + m);
        JtJ.block(0, 0, n, n) = A;
        JtJ.block(n, n, m, m) = D;
        JtJ.block(0, n, n, m) = B;
        JtJ.block(n, 0, m, n) = B.transpose();

        //        cout << JtJ << endl << endl;

        Eigen::VectorXd Jtb(n + m);
        Jtb.segment(0, n) = b1;
        Jtb.segment(n, m) = b2;


        Eigen::VectorXd x = JtJ.ldlt().solve(Jtb);
        x1 = x.segment(0, n);
        x2 = x.segment(n, m);


#endif
        //        cout << x1.transpose() << endl;
        //        cout << x2.transpose() << endl;


        for (size_t i = 0; i < numPoints; ++i)
        {
            Vec3 t  = x1.segment(i * 3, 3);
            auto& p = scene.worldPoints[i].p;
            p += t;
        }

        for (size_t i = 0; i < numCameras; ++i)
        {
            Sophus::SE3d::Tangent t = x2.segment(i * 6, 6);
            auto& se3               = scene.extrinsics[i].se3;
            se3                     = Sophus::SE3d::exp(t) * se3;
        }
    }
}


void BAPoseOnly::sbaPaper(Scene& scene, int its)
{
    using T          = double;
    using KernelType = Saiga::Kernel::BAPosePointMono<T>;
    KernelType::PoseJacobiType JrowPose;
    KernelType::PointJacobiType JrowPoint;
    KernelType::ResidualType res;

    SAIGA_BLOCK_TIMER();



    auto imageIds   = scene.validImages();
    auto numCameras = imageIds.size();
    auto numPoints  = scene.worldPoints.size();

    cout << "ba with " << numCameras << " cameras and " << numPoints << " points" << endl;


    int n = numCameras;
    int m = numPoints;



    // ======================== Variables ========================

    UType U(n);
    VType V(m);
    WType W(n, m);
    WTType WT(m, n);

    DAType da(n);
    DBType db(m);

    DAType ea(n);
    DBType eb(m);



    for (int k = 0; k < its; ++k)
    {
        eb.setZero();
        U.setZero();
        ea.setZero();
        V.setZero();

        std::vector<Eigen::Triplet<WElem>> ws1;
        std::vector<Eigen::Triplet<WTElem>> ws2;

        //        for (auto& img : scene.images)
        for (auto imgid : imageIds)
        {
            auto& img    = scene.images[imgid];
            auto& extr   = scene.extrinsics[img.extr].se3;
            auto& camera = scene.intrinsics[img.intr];

            for (auto& ip : img.monoPoints)
            {
                if (!ip)
                {
                    cout << imgid << " " << ip.wp << " " << ip.point.transpose() << endl;
                    //                                        SAIGA_ASSERT(0);
                    continue;
                }

                auto wp = scene.worldPoints[ip.wp].p;


                int i    = img.extr;
                int j    = ip.wp;
                double w = ip.weight * scene.scale();



                KernelType::evaluateResidualAndJacobian(camera, extr, wp, ip.point, w, res, JrowPose, JrowPoint);


                U.diagonal()(i).get() += (JrowPose.transpose() * JrowPose);
                V.diagonal()(j).get() += (JrowPoint.transpose() * JrowPoint);


                WElem m = JrowPose.transpose() * JrowPoint;
                ws1.emplace_back(i, j, m);
                ws2.emplace_back(j, i, m.transpose());

                //                W.setBlock(i, j, JrowPose.transpose() * JrowPoint);
                //                W2.insert(i, j) = JrowPose.transpose() * JrowPoint;


                ea(i).get() += (JrowPose.transpose() * res);
                eb(j).get() += JrowPoint.transpose() * res;
            }
        }

        W.setFromTriplets(ws1.begin(), ws1.end());
        WT.setFromTriplets(ws2.begin(), ws2.end());


        //        double lambda = 1.0 / (scene.intrinsics.front().fx * scene.intrinsics.front().fx);
        double lambda = 1.0 / (scene.scale() * scene.scale());
        //        lambda        = 1;

        for (int i = 0; i < n; ++i)
        {
            U.diagonal()(i).get() += KernelType::PoseDiaBlockType::Identity() * lambda;
        }
        for (int i = 0; i < m; ++i)
        {
            V.diagonal()(i).get() += KernelType::PointDiaBlockType::Identity() * lambda;
        }


        // tmp variables
        VType Vinv(m);
        WType Y(n, m);
        SType S(n, n);
        DAType ej(n);



        {
            SAIGA_BLOCK_TIMER();
            // Schur complement solution

            // Step 1
            // Invert V
            for (int i = 0; i < m; ++i) Vinv.diagonal()(i) = V.diagonal()(i).get().inverse();
        }

        {
            SAIGA_BLOCK_TIMER();
            // Step 2
            // Compute Y
            Y = multSparseDiag(W, Vinv);
        }

        {
            SAIGA_BLOCK_TIMER();
            // Step 3
            // Compute the Schur complement S
            // Not sure how good the sparse matrix mult is of eigen
            // maybe own implementation because the structure is well known before hand

            // TODO: this line doesn't seem to compile with every eigen version
//            S = -(Y * WT).eval();

            //            cout << "S" << endl << blockMatrixToMatrix(S.toDense()) << endl;
            //            cout << "U" << endl << blockMatrixToMatrix(U.toDenseMatrix()) << endl;

            //        S = W * WT;
            S.diagonal() = U.diagonal() + S.diagonal();

            //        cout << "Sref" << endl
            //             << (blockMatrixToMatrix(U.toDenseMatrix()) - blockMatrixToMatrix(W.toDense()) *
            //                                                              blockMatrixToMatrix(Vinv.toDenseMatrix()) *
            //                                                              blockMatrixToMatrix(WT.toDense()))
            //                    .eval()
            //             << endl;
            //        cout << "S" << endl << blockMatrixToMatrix(S.toDense()) << endl;
        }
        {
            SAIGA_BLOCK_TIMER();
            // Step 4
            // Compute the right hand side of the schur system ej
            // S * da = ej
            ej = ea + -(Y * eb);  // todo fix -
                                  //        cout << "ejref" << endl
            //             << (blockVectorToVector(ea) - blockMatrixToMatrix(Y.toDense()) * blockVectorToVector(eb)) <<
            //             endl;

            //        cout << "ej" << endl << blockVectorToVector(ej) << endl;
        }


        Eigen::SparseMatrix<double> ssparse(n * asize, n * asize);
        {
            SAIGA_BLOCK_TIMER();
            // Step 5
            // Solve the schur system for da

            auto triplets = sparseBlockToTriplets(S);

            ssparse.setFromTriplets(triplets.begin(), triplets.end());
        }
        {
            SAIGA_BLOCK_TIMER();
            //        cout << "ssparse" << endl << ssparse.toDense() << endl;

            Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
            //        Eigen::SimplicialLDLT<SType> solver;
            solver.compute(ssparse);
            Eigen::Matrix<double, -1, 1> deltaA = solver.solve(blockVectorToVector(ej));

            //        cout << "deltaA" << endl << deltaA << endl;

            // copy back into da
            for (int i = 0; i < n; ++i)
            {
                da(i) = deltaA.segment(i * asize, asize);
            }
        }
        DBType q;
        {
            SAIGA_BLOCK_TIMER();
            // Step 6
            // Substitute the solultion deltaA into the original system and
            // bring it to the right hand side
            q = eb + -WT * da;
            //        cout << "qref" << endl
            //             << (blockVectorToVector(eb) - blockMatrixToMatrix(WT.toDense()) * blockVectorToVector(da)) <<
            //             endl;

            //        cout << "q" << endl << blockVectorToVector(q) << endl;
        }
        {
            SAIGA_BLOCK_TIMER();
            // Step 7
            // Solve the remaining partial system with the precomputed inverse of V
            db = multDiagVector(Vinv, q);

#if 0
            // compute residual of linear system
            auto xa                           = blockVectorToVector(da);
            auto xb                           = blockVectorToVector(db);
            Eigen::Matrix<double, -1, 1> res1 = blockMatrixToMatrix(U.toDenseMatrix()) * xa +
                                                blockMatrixToMatrix(W.toDense()) * xb - blockVectorToVector(ea);
            Eigen::Matrix<double, -1, 1> res2 = blockMatrixToMatrix(WT.toDense()) * xa +
                                                blockMatrixToMatrix(V.toDenseMatrix()) * xb - blockVectorToVector(eb);
            cout << "Error: " << res1.norm() << " " << res2.norm() << endl;
#endif
            //        cout << "da" << endl << blockVectorToVector(da).transpose() << endl;
            //        cout << "db" << endl << blockVectorToVector(db).transpose() << endl;
        }


        Eigen::VectorXd x1, x2;
        x1 = blockVectorToVector(da);
        x2 = blockVectorToVector(db);

#if 0
        // ======================== Dense Simple Solution (only for checking the correctness) ========================
        {
            SAIGA_BLOCK_TIMER();
            n *= asize;
            m *= bsize;

            // Build the complete system matrix
            Eigen::MatrixXd M(m + n, m + n);
            M.block(0, 0, n, n) = blockDiagonalToMatrix(U);
            M.block(n, n, m, m) = blockDiagonalToMatrix(V);
            M.block(0, n, n, m) = blockMatrixToMatrix(W.toDense());
            M.block(n, 0, m, n) = blockMatrixToMatrix(W.toDense()).transpose();

            // stack right hand side
            Eigen::VectorXd b(m + n);
            b.segment(0, n) = blockVectorToVector(ea);
            b.segment(n, m) = blockVectorToVector(eb);

            // compute solution
            Eigen::VectorXd x = M.ldlt().solve(b);

            double error = (M * x - b).norm();
            cout << x.transpose() << endl;
            cout << "Dense error " << error << endl;

            x1 = x.segment(0, n);
            x2 = x.segment(n, m);

            n /= asize;
            m /= bsize;
        }
#endif

#if 1


        for (size_t i = 0; i < imageIds.size(); ++i)
        //        for(int i )
        {
            auto id                 = imageIds[i];
            Sophus::SE3d::Tangent t = x1.segment(i * 6, 6);
            auto& se3               = scene.extrinsics[id].se3;
            se3                     = Sophus::SE3d::exp(t) * se3;
        }

        for (size_t i = 0; i < numPoints; ++i)
        {
            Vec3 t  = x2.segment(i * 3, 3);
            auto& p = scene.worldPoints[i].p;
            p += t;
        }
#endif
    }
}

void BAPoseOnly::posePointSparse(Scene& scene, int its)
{
    using T          = double;
    using KernelType = Saiga::Kernel::BAPosePointMono<T>;

    KernelType::PoseJacobiType JrowPose;
    KernelType::PointJacobiType JrowPoint;
    KernelType::ResidualType res;


    int N = 0;
    for (auto& img : scene.images)
    {
        for (auto& ip : img.monoPoints)
        {
            if (ip) N++;
        }
    }

    SAIGA_BLOCK_TIMER();
    auto numCameras  = scene.extrinsics.size();
    auto numPoints   = scene.worldPoints.size();
    auto numUnknowns = numCameras * 6 + numPoints * 3;

    std::vector<KernelType::PoseDiaBlockType> diagPoseBlocks(numCameras);
    std::vector<KernelType::PointDiaBlockType> diagPointBlocks(numPoints);
    std::vector<KernelType::PosePointUpperBlockType> posePointBlocks(N);
    Eigen::VectorXd Jtb(numUnknowns);

    for (int k = 0; k < its; ++k)
    {
        Jtb.setZero();
        for (auto& b : diagPoseBlocks) b.setZero();
        for (auto& b : diagPointBlocks) b.setZero();
        for (auto& b : posePointBlocks) b.setZero();

        int n = 0;
        for (auto& img : scene.images)
        {
            for (auto& ip : img.monoPoints)
            {
                if (!ip) continue;

                auto& wp     = scene.worldPoints[ip.wp].p;
                auto& extr   = scene.extrinsics[img.extr].se3;
                auto& camera = scene.intrinsics[img.intr];

                auto poseStart  = img.extr * 6;
                auto pointStart = numCameras * 6 + ip.wp * 3;


                KernelType::evaluateResidualAndJacobian(camera, extr, wp, ip.point, ip.weight, res, JrowPose,
                                                        JrowPoint);

                diagPoseBlocks[img.extr] +=
                    (JrowPose.transpose() * JrowPose);  //.template triangularView<Eigen::Upper>();
                diagPointBlocks[ip.wp] +=
                    (JrowPoint.transpose() * JrowPoint);  //.template triangularView<Eigen::Upper>();

                posePointBlocks[n] = JrowPose.transpose() * JrowPoint;

                Jtb.segment(poseStart, 6) += JrowPose.transpose() * res;
                Jtb.segment(pointStart, 3) += JrowPoint.transpose() * res;

                n++;
            }
        }


        typedef Eigen::Triplet<T> Trip;
        std::vector<Trip> tripletList;

        Eigen::SparseMatrix<T> mat(numUnknowns, numUnknowns);  // default is column major
        //        mat.reserve(Eigen::VectorXi::Constant(numUnknowns, 6));

        for (size_t i = 0; i < numCameras; ++i)
        {
            auto starti = i * 6;
            auto startj = i * 6;
            for (int j = 0; j < 6; ++j)
            {
                for (int k = 0; k < 6; ++k)
                {
                    //                    mat.insert(starti + k, startj + j) = diagPoseBlocks[i](k, j);
                    tripletList.emplace_back(starti + k, startj + j, diagPoseBlocks[i](k, j));
                }
            }
        }
        for (size_t i = 0; i < numPoints; ++i)
        {
            auto starti = numCameras * 6 + i * 3;
            auto startj = numCameras * 6 + i * 3;
            for (int j = 0; j < 3; ++j)
            {
                for (int k = 0; k < 3; ++k)
                {
                    //                    mat.insert(starti + k, startj + j) = diagPointBlocks[i](k, j);
                    tripletList.emplace_back(starti + k, startj + j, diagPointBlocks[i](k, j));
                }
            }
        }
#if 1

        n = 0;
        for (auto& img : scene.images)
        {
            for (auto& ip : img.monoPoints)
            {
                if (!ip) continue;

                auto poseStart  = img.extr * 6;
                auto pointStart = numCameras * 6 + ip.wp * 3;

                for (int c = 0; c < 3; ++c)
                {
                    for (int r = 0; r < 6; ++r)
                    {
                        //                        mat.insert(poseStart + r, pointStart + c) = posePointBlocks[n](r, c);
                        tripletList.emplace_back(poseStart + r, pointStart + c, posePointBlocks[n](r, c));
                        tripletList.emplace_back(pointStart + c, poseStart + r, posePointBlocks[n](r, c));
                    }
                }
                n++;
            }
        }
#endif

        mat.setFromTriplets(tripletList.begin(), tripletList.end());

        //        std::ofstream strm("jtjsparse.txt");
        //        strm << mat << endl;
        //        strm.close();
        {
            //            double lambda = 1;
            double lambda = 1.0 / scene.intrinsics.front().fx;
            // lm diagonal
            for (int i = 0; i < numUnknowns; ++i)
            {
                // that's what g2o does
                mat.coeffRef(i, i) += lambda;  // * JtJ(i, i);
            }
        }
        cout << "insert done" << endl;

        Eigen::VectorXd x;

        {
            SAIGA_BLOCK_TIMER();
            //        using SolverType = Eigen::SimplicialLDLT<Eigen::SparseMatrix<T>, Eigen::Upper>;
            //            using SolverType = Eigen::SimplicialLDLT<Eigen::SparseMatrix<T>, Eigen::Upper>;
            using SolverType = Eigen::SimplicialLDLT<Eigen::SparseMatrix<T>, Eigen::Lower>;
            //            using SolverType = Eigen::ConjugateGradient<Eigen::SparseMatrix<T>, Eigen::Upper>;

            SolverType solver;
            x = solver.compute(mat).solve(Jtb);

            if (solver.info() != Eigen::Success)
            {
                cout << "decomposition failed" << endl;
                return;
            }
        }


        for (size_t i = 0; i < numCameras; ++i)
        {
            Sophus::SE3d::Tangent t = x.segment(i * 6, 6);
            auto& se3               = scene.extrinsics[i].se3;
            se3                     = Sophus::SE3d::exp(t) * se3;
        }
#if 1
        for (size_t i = 0; i < numPoints; ++i)
        {
            Vec3 t  = x.segment(numCameras * 6 + i * 3, 3);
            auto& p = scene.worldPoints[i].p;
            p += t;
        }
#endif
    }
}

}  // namespace Saiga
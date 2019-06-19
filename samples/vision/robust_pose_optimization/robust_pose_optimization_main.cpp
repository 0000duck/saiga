﻿/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */
#include "saiga/core/Core.h"
#include "saiga/core/time/all.h"
#include "saiga/core/util/fileChecker.h"
#include "saiga/vision/Eigen_Compile_Checker.h"
#include "saiga/vision/RobustPoseOptimization.h"

#include <fstream>

using namespace Saiga;

template <typename T, bool Normalized>
class RPOTest
{
   public:
    RPOTest()
    {
        load();
        if (Normalized) normalize();
    }

    void load()
    {
        // Everything is stored in double
        using TLoad    = double;
        using Vec3Load = Eigen::Matrix<TLoad, 3, 1>;
        using ObsLoad  = ObsBase<TLoad>;
        StereoCamera4 K;
        SE3 pose;
        AlignedVector<Vec3Load> wps;
        AlignedVector<ObsLoad> obs;

        std::ifstream strm(SearchPathes::data("vision/poseRefinement.dat"), std::ios::binary);
        SAIGA_ASSERT(strm.is_open());

        int wpc;
        int obsc;
        strm.read((char*)&wpc, sizeof(int));
        strm.read((char*)&obsc, sizeof(int));
        strm.read((char*)&K, sizeof(StereoCamera4));
        strm.read((char*)pose.data(), sizeof(SE3));
        wps.resize(wpc);
        obs.resize(obsc);
        for (auto& wp : wps) strm.read((char*)wp.data(), sizeof(Vec3Load));
        for (auto& ob : obs) strm.read((char*)&ob, sizeof(ObsLoad));


        this->K    = K.cast<T>();
        this->pose = pose.cast<T>();
        this->wps.resize(wpc);
        this->wps4.resize(wpc);
        this->obs.resize(obsc);
        for (auto i : Range(0, wpc))
        {
            this->wps[i]                   = wps[i].template cast<T>();
            wps4[i].template segment<3>(0) = this->wps[i];
        }
        for (auto i : Range(0, obsc))
        {
            this->obs[i] = obs[i].template cast<T>();
        }

        outlier.resize(wpc);
    }

    void normalize()
    {
        auto factor = 2.0 / (K.fx + K.fy);

        for (auto& o : obs)
        {
            o.ip = K.unproject2(o.ip);
        }
        K.bf = K.bf * factor;
        rpo.scaleThresholds(factor);
    }

    int optimize()
    {
        std::fill(outlier.begin(), outlier.end(), false);
        SE3Type p = pose;
        int inliers;
        {
            //            SAIGA_BLOCK_TIMER();
            inliers = rpo.optimizePoseRobust(wps, obs, outlier, p, K);
            //            inliers = rpo.optimizePoseRobust4(wps4, obs, outlier, p, K);
        }

        //        std::cout << "[PoseRefinement] Wps/Inliers " << wps.size() << "/" << inliers << " " << p << std::endl;
        return inliers;
    }

    using SE3Type = Sophus::SE3<T>;
    using Vec3    = Eigen::Matrix<T, 3, 1>;
    using Vec4    = Eigen::Matrix<T, 4, 1>;
    using Obs     = ObsBase<T>;
    StereoCamera4Base<T> K;
    SE3Type pose;
    AlignedVector<Vec3> wps;
    AlignedVector<Vec4> wps4;
    AlignedVector<Obs> obs;

    AlignedVector<bool> outlier;
    RobustPoseOptimization<T, Normalized> rpo;
};


int main(int, char**)
{
    Saiga::SaigaParameters saigaParameters;
    Saiga::initSample(saigaParameters);
    Saiga::initSaiga(saigaParameters);

    Saiga::Random::setSeed(93865023985);

    Saiga::EigenHelper::EigenCompileFlags flags;
    flags.create<3998735>();
    std::cout << flags << std::endl;
    RPOTest<double, true> test_float;
    RPOTest<double, false> test_double;
    std::cout << std::endl;

    //    std::cout << Kernel::huberWeight(0.5, 0.4999 * 0.4999) << std::endl << std::endl;
    //    std::cout << Kernel::huberWeight(0.5, 0.5001 * 0.5001) << std::endl << std::endl;
    //    return 0;

    int sum = 0;

    int its = 5000;
    test_double.optimize();
    test_float.optimize();
    //    auto a = measureObject("Float", its, [&]() { sum += test_float.optimize(); });
    //    auto b = measureObject("Double", its, [&]() { sum += test_double.optimize(); });

    std::cout << "Sum: " << sum << std::endl;
    //    std::cout << a.median << " " << b.median << std::endl;
    return 0;
}

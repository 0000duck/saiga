/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#pragma once
#include "saiga/vision/ceres/CeresHelper.h"
#include "saiga/vision/ceres/local_parameterization_se3.h"
#include "saiga/vision/ceres/local_parameterization_sim3.h"

#include "PoseOptimizationScene.h"

#include "ceres/autodiff_cost_function.h"
#include "ceres/loss_function.h"

namespace Saiga
{
template <typename ScalarType>
struct PoseOptimizationCeresCost
{
    // Helper function to simplify the "add residual" part for creating ceres problems
    using CostType = PoseOptimizationCeresCost;
    // Note: The first number is the number of residuals
    //       The following number sthe size of the residual blocks (without local parametrization)
    using CostFunctionType = ceres::AutoDiffCostFunction<CostType, 2, 7>;
    template <typename... Types>
    static CostFunctionType* create(Types... args)
    {
        return new CostFunctionType(new CostType(args...));
    }

    template <typename T>
    bool operator()(const T* const _extrinsics, T* _residuals) const
    {
        Eigen::Map<Sophus::SE3<T> const> const se3(_extrinsics);
        Eigen::Map<Eigen::Matrix<T, 2, 1>> residual(_residuals);

        Eigen::Matrix<T, 3, 1> wp       = _wp.cast<T>();
        auto intr                       = _intr.cast<T>();
        Eigen::Matrix<T, 2, 1> observed = _observed.cast<T>();
        Eigen::Matrix<T, 3, 1> pc;
        pc                          = se3 * wp;
        Eigen::Matrix<T, 2, 1> proj = intr.project(pc);
        residual                    = T(weight) * (observed - proj);
        return true;
    }

    PoseOptimizationCeresCost(Intrinsics4 intr, Vec2 observed, Vec3 wp, double weight = 1)
        : _intr(intr), _observed(observed), _wp(wp), weight(weight)
    {
    }

    Intrinsics4 _intr;
    Vec2 _observed;
    Vec3 _wp;
    double weight;
};


template <typename T>
OptimizationResults OptimizePoseCeres(PoseOptimizationScene<T>& scene)
{
    ceres::Problem::Options problemOptions;
    ceres::Problem problem(problemOptions);

    double huber = sqrt(2);

    ceres::LocalParameterization* camera_parameterization;
    camera_parameterization = new Sophus::test::LocalParameterizationSE3;
    problem.AddParameterBlock(scene.pose.data(), 7, camera_parameterization);

    for (int i = 0; i < scene.obs.size(); ++i)
    {
        auto o  = scene.obs[i];
        auto wp = scene.wps[i];

        auto* cost                = PoseOptimizationCeresCost<T>::create(scene.K, o.ip, wp, o.weight);
        ceres::LossFunction* loss = nullptr;
        if (huber > 0) loss = new ceres::HuberLoss(huber);
        problem.AddResidualBlock(cost, loss, scene.pose.data());
    }

    ceres::Solver::Options ceres_options;
    ceres_options.minimizer_progress_to_stdout = true;
    return ceres_solve(ceres_options, problem);
}

}  // namespace Saiga

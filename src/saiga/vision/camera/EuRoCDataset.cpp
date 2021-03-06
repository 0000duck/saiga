/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#include "EuRoCDataset.h"

#include "saiga/core/util/FileSystem.h"
#include "saiga/core/util/ProgressBar.h"
#include "saiga/core/util/file.h"
#include "saiga/core/util/fileChecker.h"
#include "saiga/core/util/tostring.h"
#include "saiga/vision/camera/TimestampMatcher.h"

#ifdef SAIGA_USE_YAML_CPP

#    include "saiga/core/util/easylogging++.h"
#    include "saiga/core/util/yaml.h"

namespace Saiga
{
// Reads csv files of the following format:
//
// #timestamp [ns],filename
// 1403636579763555584,1403636579763555584.png
// 1403636579813555456,1403636579813555456.png
// 1403636579863555584,1403636579863555584.png
// ...
std::vector<std::pair<double, std::string>> loadTimestapDataCSV(const std::string& file)
{
    auto lines = File::loadFileStringArray(file);
    File::removeWindowsLineEnding(lines);

    StringViewParser csvParser(", ");

    // timestamp - filename
    std::vector<std::pair<double, std::string>> data;
    for (auto&& l : lines)
    {
        if (l.empty()) continue;
        if (l[0] == '#') continue;

        csvParser.set(l);

        auto svTime = csvParser.next();
        if (svTime.empty()) continue;
        auto svImg = csvParser.next();
        if (svImg.empty()) continue;

        data.emplace_back(to_double(svTime), svImg);
    }
    std::sort(data.begin(), data.end());
    return data;
}

struct Associations
{
    // left and right image id
    int left, right;
    // id into gt array
    // gtlow is the closest gt index smaller and gthigh is the closest gt index larger.
    int gtlow, gthigh;
    // the interpolation value between low and high
    double gtAlpha;

    double timestamp;
};


EuRoCDataset::EuRoCDataset(const DatasetParameters& _params) : DatasetCameraBase<StereoFrameData>(_params)
{
    Load();
    computeImuDataPerFrame();
}

void EuRoCDataset::LoadImageData(StereoFrameData& data)
{
    //    std::cout << "EuRoCDataset::LoadImageData " << data.id << std::endl;
    SAIGA_ASSERT(data.grayImg.rows == 0);
    // Load if it's not loaded already

    data.grayImg.load(data.file);
    if (!params.force_monocular)
    {
        data.grayImg2.load(data.file2);
    }
}

int EuRoCDataset::LoadMetaData()
{
    std::cout << "Loading EuRoCDataset Stereo Dataset: " << params.dir << std::endl;

    auto leftImageSensor  = params.dir + "/cam0/sensor.yaml";
    auto rightImageSensor = params.dir + "/cam1/sensor.yaml";
    auto imuSensor        = params.dir + "/imu0/sensor.yaml";

    SAIGA_ASSERT(std::filesystem::exists(leftImageSensor));
    SAIGA_ASSERT(std::filesystem::exists(rightImageSensor));
    SAIGA_ASSERT(std::filesystem::exists(imuSensor));


    {
        // == Cam 0 ==
        // Load camera meta data
        YAML::Node config = YAML::LoadFile(leftImageSensor);
        SAIGA_ASSERT(config);
        SAIGA_ASSERT(!config.IsNull());

        VLOG(1) << config["comment"].as<std::string>();
        SAIGA_ASSERT(config["camera_model"].as<std::string>() == "pinhole");
        intrinsics.fps = config["rate_hz"].as<double>();
        intrinsics.model.K.coeffs(readYamlMatrix<Vec4>(config["intrinsics"]));
        auto res               = readYamlMatrix<ivec2>(config["resolution"]);
        intrinsics.imageSize.w = res(0);
        intrinsics.imageSize.h = res(1);
        // 4 parameter rad-tan model
        intrinsics.model.dis.segment<4>(0) = readYamlMatrix<Vec4>(config["distortion_coefficients"]);
        intrinsics.model.dis(4)            = 0;
        Mat4 m                             = readYamlMatrix<Mat4>(config["T_BS"]["data"]);
        extrinsics_cam0                    = SE3::fitToSE3(m);
        cam0_images                        = loadTimestapDataCSV(params.dir + "/cam0/data.csv");
    }

    {
        // == Cam 1 ==
        // Load camera meta data
        YAML::Node config = YAML::LoadFile(rightImageSensor);
        VLOG(1) << config["comment"].as<std::string>();
        SAIGA_ASSERT(config["camera_model"].as<std::string>() == "pinhole");
        intrinsics.rightModel.K.coeffs(readYamlMatrix<Vec4>(config["intrinsics"]));
        auto res                    = readYamlMatrix<ivec2>(config["resolution"]);
        intrinsics.rightImageSize.w = res(0);
        intrinsics.rightImageSize.h = res(1);
        // 4 parameter rad-tan model
        intrinsics.rightModel.dis.segment<4>(0) = readYamlMatrix<Vec4>(config["distortion_coefficients"]);
        intrinsics.rightModel.dis(4)            = 0;
        Mat4 m                                  = readYamlMatrix<Mat4>(config["T_BS"]["data"]);
        extrinsics_cam1                         = SE3::fitToSE3(m);
        cam1_images                             = loadTimestapDataCSV(params.dir + "/cam1/data.csv");
    }

    {
        // == IMU ==
        // Load camera meta data
        YAML::Node config = YAML::LoadFile(imuSensor);
        VLOG(1) << config["comment"].as<std::string>();
        Mat4 m             = readYamlMatrix<Mat4>(config["T_BS"]["data"]);
        imu.sensor_to_body = SE3::fitToSE3(m);

        imu.frequency                = config["rate_hz"].as<double>();
        imu.omega_sigma              = config["gyroscope_noise_density"].as<double>();
        imu.omega_random_walk        = config["gyroscope_random_walk"].as<double>();
        imu.acceleration_sigma       = config["accelerometer_noise_density"].as<double>();
        imu.acceleration_random_walk = config["accelerometer_random_walk"].as<double>();

        VLOG(1) << imu;
    }



    {
        // == Ground truth position ==

        auto sensorFile = params.dir + "/" + "state_groundtruth_estimate0/data.csv";


        auto lines = File::loadFileStringArray(sensorFile);
        StringViewParser csvParser(", ");
        std::vector<double> gtTimes;
        for (auto&& l : lines)
        {
            if (l.empty()) continue;
            if (l[0] == '#') continue;
            csvParser.set(l);

            auto svTime = csvParser.next();
            if (svTime.empty()) continue;

            Vec3 data;
            for (int i = 0; i < 3; ++i)
            {
                auto sv = csvParser.next();
                SAIGA_ASSERT(!sv.empty());
                data(i) = to_double(sv);
            }

            Vec4 dataq;
            for (int i = 0; i < 4; ++i)
            {
                auto sv = csvParser.next();
                SAIGA_ASSERT(!sv.empty());
                dataq(i) = to_double(sv);
            }

            Quat q;
            q.x() = dataq(0);
            q.y() = dataq(1);
            q.z() = dataq(2);
            q.w() = dataq(3);


            auto time = to_double(svTime);
            gtTimes.push_back(time);
            ground_truth.emplace_back(time, SE3(q, data));
        }

        YAML::Node config = YAML::LoadFile(params.dir + "/state_groundtruth_estimate0/sensor.yaml");
        Mat4 m            = readYamlMatrix<Mat4>(config["T_BS"]["data"]);
        extrinsics_gt     = SE3::fitToSE3(m);

        std::sort(ground_truth.begin(), ground_truth.end(), [](auto a, auto b) { return a.first < b.first; });
    }

    groundTruthToCamera = extrinsics_gt.inverse() * extrinsics_cam0;

    //    std::cout << "Body: " << extrinsics_gt << std::endl;
    //    std::cout << "Left: " << extrinsics_cam0 << std::endl;
    //    std::cout << "Right: " << extrinsics_cam1 << std::endl;

    //    intrinsics.left_to_right = extrinsics_cam1 * extrinsics_cam0.inverse();
    intrinsics.left_to_right = extrinsics_cam1.inverse() * extrinsics_cam0;
    //    std::cout << "Left->Right: " << intrinsics.left_to_right << std::endl;

    {
        // == Imu Data ==
        // Format:
        //   timestamp [ns],
        //   w_RS_S_x [rad s^-1],w_RS_S_y [rad s^-1],w_RS_S_z [rad s^-1],
        //   a_RS_S_x [m s^-2],a_RS_S_y [m s^-2],a_RS_S_z [m s^-2]
        auto sensorFile = params.dir + "/" + "imu0/data.csv";
        auto lines      = File::loadFileStringArray(sensorFile);
        StringViewParser csvParser(", ");

        for (auto&& l : lines)
        {
            if (l.empty()) continue;
            if (l[0] == '#') continue;
            csvParser.set(l);

            auto svTime = csvParser.next();
            if (svTime.empty()) continue;
            // time is given in nano seconds
            auto time = to_double(svTime) / 1e9;

            Vec3 omega;
            for (int i = 0; i < 3; ++i)
            {
                auto sv = csvParser.next();
                SAIGA_ASSERT(!sv.empty());
                omega(i) = to_double(sv);
            }

            Vec3 acceleration;
            for (int i = 0; i < 3; ++i)
            {
                auto sv = csvParser.next();
                SAIGA_ASSERT(!sv.empty());
                acceleration(i) = to_double(sv);
            }
            imuData.emplace_back(omega, acceleration, time);
        }
    }



    std::cout << "Found " << cam1_images.size() << " images and " << ground_truth.size()
              << " ground truth meassurements." << std::endl;

    SAIGA_ASSERT(intrinsics.imageSize == intrinsics.rightImageSize);
    VLOG(1) << intrinsics;



    std::vector<Associations> assos;
    // =========== Associate ============
    {
        // extract timestamps so the association matcher works
        std::vector<double> left_timestamps, right_timestamps;
        std::vector<double> gt_timestamps;

        for (auto i : cam0_images) left_timestamps.push_back(i.first);
        for (auto i : cam1_images) right_timestamps.push_back(i.first);
        for (auto i : ground_truth) gt_timestamps.push_back(i.first);


        for (int i = 0; i < cam0_images.size(); ++i)
        {
            Associations a;
            a.left                 = i;
            a.timestamp            = left_timestamps[i];
            a.right                = TimestampMatcher::findNearestNeighbour(left_timestamps[i], right_timestamps);
            auto [id1, id2, alpha] = TimestampMatcher::findLowHighAlphaNeighbour(left_timestamps[i], gt_timestamps);
            a.gtlow                = id1;
            a.gthigh               = id2;
            a.gtAlpha              = alpha;

            if (a.right == -1 || a.gtlow == -1 || a.gthigh == -1) continue;
            assos.push_back(a);
        }
    }



    // ==== Actual Image Loading ====
    {
        SAIGA_ASSERT(params.startFrame < (int)assos.size());
        assos.erase(assos.begin(), assos.begin() + params.startFrame);

        if (params.maxFrames >= 0)
        {
            assos.resize(std::min((size_t)params.maxFrames, assos.size()));
        }
        params.maxFrames = assos.size();


        int N = assos.size();
        frames.resize(N);

        for (int i = 0; i < N; ++i)
        {
            auto a      = assos[i];
            auto& frame = frames[i];
            frame.id    = i;
            if (a.gtlow >= 0 && a.gthigh >= 0 && a.gtlow != a.gthigh)
            {
                frame.groundTruth = slerp(ground_truth[a.gtlow].second, ground_truth[a.gthigh].second, a.gtAlpha);
            }
            frame.timeStamp = cam0_images[a.left].first / 1e9;



            frame.file  = params.dir + "/cam0/data/" + cam0_images[a.left].second;
            frame.file2 = params.dir + "/cam1/data/" + cam1_images[a.right].second;
        }
        return N;
    }
}



}  // namespace Saiga

#endif

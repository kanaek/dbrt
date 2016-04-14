/*
 * This is part of the Bayesian Robot Tracking
 *
 * Copyright (c) 2015 Max Planck Society,
 * 				 Autonomous Motion Department,
 * 			     Institute for Intelligent Systems
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License License (GNU GPL). A copy of the license can be found in the LICENSE
 * file distributed with this source code.
 */

/**
 * \file arm_robot_emulator_node.cpp
 * \date March 2016
 * \author Jan Issac (jan.issac@gmail.com)
 */

#include <memory>
#include <thread>
#include <functional>

#include <cv.h>
#include <cv_bridge/cv_bridge.h>

#include <ros/ros.h>
//#include <pcl_ros/point_cloud.h>
#include <sensor_msgs/Image.h>

#include <fl/util/profiling.hpp>

#include <dbot/util/rigid_body_renderer.hpp>
#include <dbot/util/virtual_camera_data_provider.hpp>
#include <dbot_ros/tracker_node.h>
#include <dbot_ros/tracker_publisher.h>
#include <dbot_ros/utils/ros_interface.hpp>
#include <dbrt/robot_state.hpp>
#include <dbrt/util/urdf_object_loader.hpp>
#include <dbrt/util/robot_emulator.hpp>

class ArmRobotAnimator : public dbrt::RobotAnimator
{
public:
    ArmRobotAnimator() : t_(0.) {}
    virtual void animate(const Eigen::VectorXd& current,
                         double dt,
                         double dilation,
                         Eigen::VectorXd& next)
    {
        t_ += dt;

        // left arm
        for (int i = 6; i < 6 + 7; ++i)
        {
            next[i] =
                current[i] + 0.1 * dt / dilation * std::sin(t_ / dilation);
        }

        // right arm
        for (int i = 6 + 7 + 8; i < 6 + 2 * 7 + 8; ++i)
        {
            next[i] =
                current[i] + 0.1 * dt / dilation * std::sin(t_ / dilation);
        }
    }

protected:
    double t_;
};


/**
 * \brief Node entry point
 */
int main(int argc, char** argv)
{
    ros::init(argc, argv, "arm_robot_emulator");
    ros::NodeHandle nh("~");

    // parameter shorthand prefix
    std::string prefix = "arm_robot_emulator/";


    /* ------------------------------ */
    /* - Setup camera data          - */
    /* ------------------------------ */
    auto camera_downsampling_factor =
            ri::read<double>(prefix + "camera_downsampling_factor", nh);
    auto camera_frame_id = ri::read<std::string>("camera_frame_id", nh);

    auto camera_data = std::make_shared<dbot::CameraData>(
        std::make_shared<dbot::VirtualCameraDataProvider>(
            camera_downsampling_factor, "/" + camera_frame_id));


    /* ------------------------------ */
    /* - Create the robot kinematics- */
    /* - and robot mesh model       - */
    /* ------------------------------ */

    auto robot_description =
            ri::read<std::string>("robot_description", ros::NodeHandle());
    auto robot_description_package_path =
            ri::read<std::string>("robot_description_package_path", nh);
    auto rendering_root_left =
            ri::read<std::string>("rendering_root_left", nh);
    auto rendering_root_right =
            ri::read<std::string>("rendering_root_right", nh);

    std::shared_ptr<KinematicsFromURDF> urdf_kinematics(
                new KinematicsFromURDF(robot_description,
                                       robot_description_package_path,
                                       rendering_root_left,
                                       rendering_root_right,
                                       camera_frame_id));

    auto object_model = std::make_shared<dbot::ObjectModel>(
        std::make_shared<dbrt::UrdfObjectModelLoader>(urdf_kinematics), false);



    /* ------------------------------ */
    /* - Robot renderer             - */
    /* ------------------------------ */
    auto renderer = std::make_shared<dbot::RigidBodyRenderer>(
        object_model->vertices(),
        object_model->triangle_indices(),
        camera_data->camera_matrix(),
        camera_data->resolution().height,
        camera_data->resolution().width);

    /* ------------------------------ */
    /* - Our state representation   - */
    /* ------------------------------ */
    dbrt::RobotState<>::kinematics_ = urdf_kinematics;
    dbrt::RobotState<>::kinematics_mutex_ = std::make_shared<std::mutex>();
    typedef dbrt::RobotState<> State;

    /* ------------------------------ */
    /* - Setup Simulation           - */
    /* ------------------------------ */
    ROS_INFO("Creating robot emulator... ");

    auto robot_animator =
        std::shared_ptr<dbrt::RobotAnimator>(new ArmRobotAnimator());

    auto joints = ri::read<std::vector<double>>(prefix + "initial_state", nh);

    State state;
    state = Eigen::Map<Eigen::VectorXd>(joints.data(), joints.size());


    auto joint_rate = ri::read<double>(prefix + "joint_sensor_rate", nh);
    auto image_rate = ri::read<double>(prefix + "visual_sensor_rate", nh);
    auto dilation = ri::read<double>(prefix + "dilation", nh);
    auto visual_sensor_delay =
            ri::read<double>(prefix + "visual_sensor_delay", nh);

    dbrt::RobotEmulator<State> robot(object_model,
                                     urdf_kinematics,
                                     renderer,
                                     camera_data,
                                     robot_animator,
                                     joint_rate,  // joint sensor rate
                                     image_rate,  // visual sensor rate
                                     dilation,
                                     visual_sensor_delay,
                                     state);

    /* ------------------------------ */
    /* - Run emulator node          - */
    /* ------------------------------ */
    ROS_INFO("Starting robot emulator ... ");
    robot.run();

    ros::AsyncSpinner spinner(4);
    spinner.start();

    ROS_INFO("Robot emulator running ... ");
    ROS_INFO("Use RETURN to toggle between pause/resume."
             "To explicitly pause the emulator type 'pause' and to resule the "
             "emulator enter 'resume'.");
    while(ros::ok())
    {
        std::string cmd;
        std::getline(std::cin, cmd);
        if (cmd == "pause")
        {
            robot.pause();
        }
        else if(cmd == "resume")
        {
            robot.resume();
        }
        else
        {
            robot.toggle_pause();
        }
    }
    // ros::spin();

    ROS_INFO("Shutting down ...");
    robot.shutdown();

    return 0;
}
/*
 * This is part of the Bayesian Object Tracking (bot),
 * (https://github.com/bayesian-object-tracking)
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
 * \file fusion_tracker_simulation_node.hpp
 * \date January 2016
 * \author Jan Issac (jan.issac@gmail.com)
 */

#include <cv.h>
#include <cv_bridge/cv_bridge.h>
#include <dbot/builder/particle_tracker_builder.h>
#include <dbot/rigid_body_renderer.h>
#include <dbot/virtual_camera_data_provider.h>
#include <dbot_ros/tracker_publisher.h>
#include <dbot_ros/util/data_set_camera_data_provider.h>
#include <dbot_ros/util/ros_interface.h>
#include <dbot_ros/util/tracking_dataset.h>
#include <dbrt/builder/rotary_tracker_builder.h>
#include <dbrt/factory/visual_tracker_factory.h>
#include <dbrt/fusion_tracker.h>
#include <dbrt/fusion_tracker_node.h>
#include <dbrt/robot_state.h>
#include <dbrt/robot_tracker.hpp>
#include <dbrt/robot_tracker_publisher.h>
#include <dbrt/rotary_tracker.hpp>
#include <dbrt/rotary_tracker.hpp>
#include <dbrt/util/urdf_object_loader.hpp>
#include <dbrt/util/virtual_robot.h>
#include <dbrt/visual_tracker.hpp>
#include <fl/util/profiling.hpp>
#include <functional>
#include <memory>
#include <pcl_ros/point_cloud.h>
#include <ros/ros.h>
#include <sensor_msgs/Image.h>
#include <thread>

/**
 * \brief Create a gaussian filter tracking the robot joints based on joint
 *     measurements
 * \param prefix
 *     parameter prefix, e.g. fusion_tracker
 * \param kinematics
 *     URDF robot kinematics
 */
std::shared_ptr<dbrt::RotaryTracker> create_rotary_tracker(
    const std::string& prefix,
    const int& joint_count)
{
    ros::NodeHandle nh("~");

    typedef dbrt::RotaryTracker Tracker;

    /* ------------------------------ */
    /* - State transition function  - */
    /* ------------------------------ */
    dbrt::FactorizedTransitionBuilder<Tracker>::Parameters
        transition_parameters;

    // linear state transition parameters
    nh.getParam(prefix + "joint_transition/joint_sigmas",
                transition_parameters.joint_sigmas);
    nh.getParam(prefix + "joint_transition/bias_sigmas",
                transition_parameters.bias_sigmas);
    nh.getParam(prefix + "joint_transition/bias_factors",
                transition_parameters.bias_factors);
    transition_parameters.joint_count = joint_count;

    auto transition_builder =
        std::make_shared<dbrt::FactorizedTransitionBuilder<Tracker>>(
            (transition_parameters));

    /* ------------------------------ */
    /* - Observation model          - */
    /* ------------------------------ */
    dbrt::RotarySensorBuilder<Tracker>::Parameters sensor_parameters;

    nh.getParam(prefix + "joint_observation/joint_sigmas",
                sensor_parameters.joint_sigmas);
    sensor_parameters.joint_count = joint_count;

    auto rotary_sensor_builder =
        std::make_shared<dbrt::RotarySensorBuilder<Tracker>>(sensor_parameters);

    /* ------------------------------ */
    /* - Build the tracker          - */
    /* ------------------------------ */
    auto tracker_builder = dbrt::RotaryTrackerBuilder<Tracker>(
        joint_count, transition_builder, rotary_sensor_builder);

    return tracker_builder.build();
}

/**
 * \brief Node entry point
 */
int main(int argc, char** argv)
{
    ros::init(argc, argv, "fusion_tracker_simulation");
    ros::NodeHandle nh("~");

    // parameter shorthand prefix
    std::string prefix = "fusion_tracker/";

    /* ------------------------------ */
    /* - Create the robot kinematics- */
    /* - and robot mesh model       - */
    /* ------------------------------ */

    auto kinematics = std::make_shared<KinematicsFromURDF>();
    auto mesh_model = std::make_shared<dbot::ObjectModel>(
        std::make_shared<dbrt::UrdfObjectModelLoader>(kinematics), false);

    kinematics->print_joints();
    kinematics->print_links();

    /* ------------------------------ */
    /* - Setup camera data          - */
    /* ------------------------------ */
    int downsampling_factor;
    nh.getParam("downsampling_factor", downsampling_factor);
    auto camera_data = std::make_shared<dbot::CameraData>(
        std::make_shared<dbot::VirtualCameraDataProvider>(downsampling_factor,
                                                          "/XTION"));

    /* ------------------------------ */
    /* - Robot renderer             - */
    /* ------------------------------ */
    auto renderer = std::make_shared<dbot::RigidBodyRenderer>(
        mesh_model->vertices(),
        mesh_model->triangle_indices(),
        camera_data->camera_matrix(),
        camera_data->resolution().height,
        camera_data->resolution().width);

    /* ------------------------------ */
    /* - Our state representation   - */
    /* ------------------------------ */
    dbrt::RobotState<>::kinematics_ = kinematics;

    /// \todo: somehow the two lines here make it kind of work...
    kinematics->InitKDLData(Eigen::VectorXd::Zero(kinematics->num_joints()));
    std::cout << kinematics->GetLinkPosition(3) << std::endl;

    typedef dbrt::RobotState<> State;

    /* ------------------------------ */
    /* - Tracker publisher          - */
    /* ------------------------------ */
    auto tracker_publisher = std::shared_ptr<dbot::TrackerPublisher<State>>(
        new dbrt::RobotPublisher<State>(
            urdf_kinematics, renderer, "/estimated"));

    /* ------------------------------ */
    /* - Create Tracker and         - */
    /* - tracker publisher          - */
    /* ------------------------------ */
            pre, urdf_kinematics, object_model, camera_data);
            ROS_INFO("creating trackers ... ");
            create_joint_robot_tracker(pre, urdf_kinematics);

            auto visual_tracker = dbrt::create_visual_tracker(
                pre, kinematics, mesh_model, camera_data);
            auto rotary_tracker =
                create_rotary_tracker(pre, kinematics->num_joints());
            dbrt::FusionTracker fusion_tracker(rotary_tracker, visual_tracker);

            auto kinematics_for_publisher =
                std::make_shared<KinematicsFromURDF>();
            urdf_kinematics, renderer, "/estimated"));
            /* ------------------------------ */
            /* - Setup Simulation           - */
            /* ------------------------------ */
            ROS_INFO("setting up simulation ... ");
            auto simulation_camera_data = std::make_shared<dbot::CameraData>(
                std::make_shared<dbot::VirtualCameraDataProvider>(1, "/XTION"));
            auto simulation_renderer =
                std::make_shared<dbot::RigidBodyRenderer>(
                    mesh_model->vertices(),
                    mesh_model->triangle_indices(),
                    simulation_camera_data->camera_matrix(),
                    simulation_camera_data->resolution().height,
                    simulation_camera_data->resolution().width);

            std::vector<double> joints;
            nh.getParam("simulation/initial_state", joints);

            State state;
            state = Eigen::Map<Eigen::VectorXd>(joints.data(), joints.size());

            ROS_INFO("creating virtual robot ... ");
            dbrt::VirtualRobot<State> robot(mesh_model,
                                            kinematics,
                                            simulation_renderer,
                                            simulation_camera_data,
                                            1000.,  // joint sensor rate
                                            30,     // visual sensor rate
                                            state);

            // register obsrv callbacks
            robot.joint_sensor_callback([&](const State& state) {
                fusion_tracker.joints_obsrv_callback(state);
            });

            robot.image_sensor_callback(
                [&](const sensor_msgs::Image& ros_image) {
                    fusion_tracker.image_obsrv_callback(ros_image);
                });

            /* ------------------------------ */
            /* - Initialize                 - */
            /* ------------------------------ */
            fusion_tracker.initialize({robot.state()});

            /* ------------------------------ */
            /* - Tracker publisher          - */
            /* ------------------------------ */
            //    auto tracker_publisher =
            //    std::shared_ptr<dbot::TrackerPublisher<State>>(
            //        new dbrt::RobotPublisher<State>(
            //            urdf_kinematics, renderer, "/estimated"));

            /* ------------------------------ */
            /* - Create tracker node        - */
            /* ------------------------------ */

            //    dbot::TrackerNode<dbrt::RbcParticleFilterRobotTracker>
            //    tracker_node(
            //        particle_robot_tracker, tracker_publisher);
            //    particle_robot_tracker->initialize({robot.state()});

            /* ------------------------------ */
            /* - Run tracker node           - */
            /* ------------------------------ */
            ROS_INFO("Starting robot ... ");
            ros::AsyncSpinner spinner(4);
            spinner.start();

            robot.run();
            ROS_INFO("Robot running ... ");

            fusion_tracker.run();

            ros::Rate visualization_rate(24);

            while (ros::ok())
            {
                visualization_rate.sleep();
                auto current_state = fusion_tracker.current_state();

                //        tracker_node.tracking_callback(robot.observation());
                tracker_publisher->publish(
                    current_state, robot.observation(), camera_data);

                ros::spinOnce();
            }

            ROS_INFO("Shutting down ...");

            fusion_tracker.shutdown();
            robot.shutdown();

            return 0;
}

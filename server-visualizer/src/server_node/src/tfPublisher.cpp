/**
 * @file /src/tfPublisher.cpp
 *
 * @brief Ros communication central!
 *
 * @date February 2011
 **/

/*****************************************************************************
** Includes
*****************************************************************************/

#include <ros/ros.h>
#include <ros/network.h>
#include <string>
#include <std_msgs/String.h>
#include <sstream>
#include <tf/transform_datatypes.h>
#include "../include/server_node/header.h"
#include "../include/server_node/tfPublisher.hpp"

/*****************************************************************************
** Namespaces
*****************************************************************************/

namespace server_node {

/*****************************************************************************
** Implementation
*****************************************************************************/

tfPublisher::tfPublisher(int argc, char** argv,  Calib *veloCalib, CConfig *config):
	init_argc(argc),
    init_argv(argv),
    veloCalib(veloCalib),
    config(config)
    {}

tfPublisher::~tfPublisher() {
    if(ros::isStarted()) {
      ros::shutdown(); // explicitly needed since we use ros::start();
      ros::waitForShutdown();
    }
    wait();
}

bool tfPublisher::init() {
    ros::init(init_argc,init_argv,"tf_publisher");
    if ( ! ros::master::check() ) {
        return false;
    }
    ros::start(); // explicitly needed since our nodehandle is going out of scope.
    // 监听Odom数据
    this->sub_ = this->nh.subscribe("/odom", 1, &tfPublisher::odomCallback, this);
    // 读取GPS到车体的标定参数
    FILE *f = std::fopen(config->gpsCalib[config->curDataset].toStdString().c_str(), "r");
    fscanf(f, "rot %lf %lf %lf\n", &gpsCalib.rot[0], &gpsCalib.rot[1], &gpsCalib.rot[2]);
    fscanf(f, "shv %lf %lf %lf\n", &gpsCalib.shv[0], &gpsCalib.shv[1], &gpsCalib.shv[2]);
    std::fclose(f);

    start();
    return true;
}

void tfPublisher::run() {

}

void tfPublisher::odomCallback(const nav_msgs::Odometry& odomData)
{
    // ---------------1. publish the transform over tf (velodyne - odom)--------------
    geometry_msgs::TransformStamped odom_trans;
    odom_trans.header.stamp = odomData.header.stamp;
    odom_trans.header.frame_id = "odom";        // GPS坐标系
    odom_trans.child_frame_id = "base_link";    // base_link
    odom_trans.header.seq = countHistoryTf;
    tf::Quaternion _q(odomData.pose.pose.orientation.x,
                      odomData.pose.pose.orientation.y,
                      odomData.pose.pose.orientation.z,
                      odomData.pose.pose.orientation.w);
    tf::Matrix3x3 _m(_q);
    double roll, pitch, heading;
    _m.getRPY(roll, pitch, heading);
    odom_trans.transform.rotation =  odomData.pose.pose.orientation;
    double gpsCalibX = this->gpsCalib.shv[0] * sin(heading) + this->gpsCalib.shv[1] * cos(heading);
    double gpsCalibY = -this->gpsCalib.shv[0] * cos(heading) + this->gpsCalib.shv[1] * sin(heading);
    // 将GPS对齐到车体后轴中心(base_link),要加上GPS的外参偏移量
    odom_trans.transform.translation.x = odomData.pose.pose.position.x - gpsCalibX;
    odom_trans.transform.translation.y = odomData.pose.pose.position.y - gpsCalibY;
    odom_trans.transform.translation.z = odomData.pose.pose.position.z;
    //send the transform
    this->tfBroadcaster.sendTransform(odom_trans);

    // ---------------2. publish the transform over tf (velodyne - base_link)--------------
    geometry_msgs::TransformStamped car_trans;
    car_trans.header.stamp =  odomData.header.stamp;
    car_trans.header.frame_id = "base_link";    // base_link是车体后轴中心
    car_trans.child_frame_id = "velodyne";
    car_trans.header.seq = countHistoryTf;
    car_trans.transform.translation.x = veloCalib->shv[1];
    car_trans.transform.translation.y = -veloCalib->shv[0];
    car_trans.transform.translation.z = veloCalib->shv[2];
    car_trans.transform.rotation = tf::createQuaternionMsgFromRollPitchYaw(veloCalib->rot[0],
                                                                           veloCalib->rot[1],
                                                                           veloCalib->rot[2]);
    this->carTfBroadcaster.sendTransform(car_trans);

    // ---------------3. publish the transform over tf (lidar_bbox - base_link)--------------
    // LiDAR 目标检测包围框的坐标系(应该是后轴中心,右x前y),需要旋转到ROS标准的base_link坐标系下(前x左y)
    geometry_msgs::TransformStamped lidar_bbox_trans;
    lidar_bbox_trans.header.stamp =  odomData.header.stamp;
    lidar_bbox_trans.header.frame_id = "base_link";    // base_link是车体后轴中心
    lidar_bbox_trans.child_frame_id = "lidar_bbox";
    lidar_bbox_trans.header.seq = countHistoryTf;
    lidar_bbox_trans.transform.translation.x = 0;//gpsCalib.shv[1];
    lidar_bbox_trans.transform.translation.y = 0;
    lidar_bbox_trans.transform.translation.z = 0;
    lidar_bbox_trans.transform.rotation = tf::createQuaternionMsgFromRollPitchYaw(0,0,-M_PI_2);
    this->lidarBboxTfBroadcaster.sendTransform(lidar_bbox_trans);

    ++ countHistoryTf;
}

}  // namespace server_node

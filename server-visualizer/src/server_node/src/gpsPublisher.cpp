/**
 * @file /src/gpsPublisher.cpp
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
#include "../include/server_node/header.h"
#include "../include/server_node/gpsPublisher.hpp"

/*****************************************************************************
** Namespaces
*****************************************************************************/

namespace server_node {

/*****************************************************************************
** Implementation
*****************************************************************************/

gpsPublisher::gpsPublisher(int argc, char** argv,  Calib *veloCalib, CConfig *config):
	init_argc(argc),
    init_argv(argv),
    veloCalib(veloCalib),
    config(config)
    {}

gpsPublisher::~gpsPublisher() {
    if(ros::isStarted()) {
      ros::shutdown(); // explicitly needed since we use ros::start();
      ros::waitForShutdown();
    }
    wait();
}

bool gpsPublisher::init() {
    ros::init(init_argc,init_argv,"gpsimu_publisher");
    if ( ! ros::master::check() ) {
        return false;
    }
    ros::start(); // explicitly needed since our nodehandle is going out of scope.
    // 定义IMU数据的消息发布
    this->imuPub = this->nh.advertise<sensor_msgs::Imu>("/imu", 100);
    // 定义GPS数据的消息发布
    this->gpsPub = this->nh.advertise<sensor_msgs::NavSatFix>("/gps", 100);
    // 定义Odom数据的消息发布
    this->odomPub = this->nh.advertise<nav_msgs::Odometry>("/odom", 100);
    // 监听velodyne数据
    this->sub_ = this->nh.subscribe("/velodyne_points", 1, &gpsPublisher::veloCallback, this);
    // 读取gps数据
    int scanfRet = 0;
    FILE *gpsf = std::fopen(config->gpsFile[config->curDataset].toStdString().c_str(), "r");
    fscanf(gpsf, "%*[^\n]"); // 第一行标题，读入并忽略
    long long ts;
    double latitude, longitude, altitude;
    double NSV1,NSV2;  // 收星数，暂时没用
    scanfRet = fscanf(gpsf, "%lld,%lf,%lf,%lf,%lf,%lf\n", &ts, &latitude, &longitude, &altitude, &NSV1, &NSV2);
    while (scanfRet != EOF) {
        scanfRet = fscanf(gpsf, "%lld,%lf,%lf,%lf,%lf,%lf\n", &ts, &latitude, &longitude, &altitude, &NSV1, &NSV2);
        while (scanfRet == 0) { // 存在非法字符，跳过该数据条目
            fscanf(gpsf, "%*[^\n]");
            scanfRet = fscanf(gpsf, "%lld,%lf,%lf,%lf,%lf,%lf\n", &ts, &latitude, &longitude, &altitude, &NSV1, &NSV2);
        }
        gpsVec.push_back(GPSDATA(ts, latitude, longitude, altitude, NSV1, NSV2));
    }
    std::fclose(gpsf);

    // 设置GPS原点
    config->coordConvertor.SetCoordinateParameters(CoordinateConvertion::_SELFDEF, gpsVec[0].latitude, gpsVec[0].longitude);

    // 读取imu数据
    FILE *imuf = std::fopen(config->imuFile[config->curDataset].toStdString().c_str(), "r");
    fscanf(imuf, "%*[^\n]"); // 第一行标题，读入并忽略
    double heading, pitch, roll;
    double vEast, vNorth, vUp;
    scanfRet = fscanf(imuf, "%lld,%lf,%lf,%lf,%lf,%lf,%lf\n", &ts, &heading, &pitch, &roll, &vEast, &vNorth, &vUp);
    while (scanfRet != EOF) {
        scanfRet = fscanf(imuf, "%lld,%lf,%lf,%lf,%lf,%lf,%lf\n", &ts, &heading, &pitch, &roll, &vEast, &vNorth, &vUp);
        while (scanfRet == 0) { // 存在非法字符，跳过该数据条目
            fscanf(imuf, "%*[^\n]");
            scanfRet = fscanf(imuf, "%lld,%lf,%lf,%lf,%lf,%lf,%lf\n", &ts, &heading, &pitch, &roll, &vEast, &vNorth, &vUp);
        }
        imuVec.push_back(IMUDATA(ts, heading, pitch, roll, vEast, vNorth, vUp));
    }
    std::fclose(imuf);
    countGps = countImu = 0;
    start();
    return true;
}

void gpsPublisher::run() {
    // GPS/IMU数据帧率
    ros::Rate loop_rate(100);
    std_msgs::Header header;
    ros::Time _ts;
    while ( this->nh.ok() ) {
        // 激光播放启动比较慢，等待收到第一帧激光数据后，再开始播放数据
        if (this->hasGetSubscribe) {
            // ---------------1. 填充IMU数据-------------
            sensor_msgs::Imu imuData;
            _ts.fromNSec(imuVec[this->countImu].ts*1E6 + this->deltaUnixTs);
            imuData.header.stamp = _ts;
            imuData.header.frame_id = "base_link";
            // 星网宇达坐标系下,heading是与北的夹角,顺时针正方向; 右x前y
            // ROS坐标系下,yaw是逆时针正方向,前x左y
            imuData.orientation = tf::createQuaternionMsgFromRollPitchYaw(imuVec[this->countImu].roll/180.0*M_PI,
                                                                          -imuVec[this->countImu].pitch/180.0*M_PI,
                                                                          -imuVec[this->countImu].heading/180.0*M_PI);
            double _heading = -imuVec[this->countImu].heading/180.0*M_PI;
            imuData.angular_velocity.x = imuVec[this->countImu].vEast * cos(_heading) - imuVec[this->countImu].vNorth * sin(_heading);
            imuData.angular_velocity.y = imuVec[this->countImu].vEast * sin(_heading) + imuVec[this->countImu].vNorth * cos(_heading);
            imuData.angular_velocity.z = imuVec[this->countImu].vUp;
            imuData.linear_acceleration.x = imuData.linear_acceleration.y = imuData.linear_acceleration.z = 0; // 这些数值暂时用不到，都填充为0
            // If you have no estimate for one of the data elements (e.g. your IMU doesn't produce an orientation
            // estimate), please set element 0 of the associated covariance matrix to -1
            imuData.angular_velocity_covariance[0] = -1;
            imuData.linear_acceleration_covariance[0] = -1;
            imuPub.publish(imuData);

            // ---------------2. 填充GPS数据-------------
            sensor_msgs::NavSatFix gpsData;
            _ts.fromNSec(gpsVec[this->countGps].ts*1E6 + this->deltaUnixTs);
            gpsData.header.stamp = _ts;
            gpsData.header.frame_id = "base_link";
            gpsData.status.status = gpsVec[this->countGps].NSV1;
            gpsData.latitude = gpsVec[this->countGps].latitude;
            gpsData.longitude = gpsVec[this->countGps].longitude;
            gpsData.altitude = gpsVec[this->countGps].altitude;
            gpsPub.publish(gpsData);

            // ---------------3. 填充Odom数据-------------
            std::array<double, 2> WGS84Position{(gpsData.latitude), (gpsData.longitude)};
            std::array<double, 2> result{wgs84::toCartesian(WGS84Reference, WGS84Position)};
            double lbh[3], xyz[3];
            lbh[0] = gpsData.latitude; lbh[1] = gpsData.longitude; lbh[2] = gpsData.altitude;
            config->coordConvertor.GPS84LBH_LocalXYZ(lbh, xyz);

            nav_msgs::Odometry odomData;
            odomData.header.stamp = gpsData.header.stamp;
            odomData.header.frame_id = "odom";
            odomData.child_frame_id = gpsData.header.frame_id;
//            odomData.pose.pose.position.x = result[1];
//            odomData.pose.pose.position.y = -result[0];
//            odomData.pose.pose.position.z = gpsData.altitude;
            odomData.pose.pose.position.x = xyz[0];
            odomData.pose.pose.position.y = -xyz[1];
            odomData.pose.pose.position.z = xyz[2];
            odomData.pose.pose.orientation = imuData.orientation;
            odomData.twist.twist.linear.x = imuData.angular_velocity.x;
            odomData.twist.twist.linear.y = imuData.angular_velocity.y;
            odomData.twist.twist.linear.z = imuData.angular_velocity.z;
            odomPub.publish(odomData);

            countGps = std::min(countGps+1, int(gpsVec.size()-1));
            countImu = std::min(countImu+1, int(imuVec.size()-1));
        }
        loop_rate.sleep();
        ros::spinOnce();
    }
    Q_EMIT rosShutdown(); // used to signal the gui for a shutdown (useful to roslaunch)
}

long long gpsPublisher::unixTime2DateTime(const ros::Time& stamp)
{
    // 输入的unix时间戳，输出timeofday时间戳，单位毫秒
    std::time_t rawTime = static_cast<time_t>(stamp.sec);
    struct tm* timeInfo = localtime(&rawTime);
    long long dateTime = timeInfo->tm_hour*3600+timeInfo->tm_min*60+timeInfo->tm_sec;
    dateTime = dateTime * 1000 + stamp.nsec / 1E6;
    return dateTime;
}

void gpsPublisher::veloCallback(const sensor_msgs::PointCloud2& msg)
{
    // 监听激光的topic，每隔一段时间同步一下当前节点和激光的播放速度
    // 每隔几秒校准一次gps/imu和激光的时间,校准太频繁会卡顿
    if (countGps % 500 < 5) {  // 5秒  Velodyne比gps频率慢,所以刚好触发velodyne callback的时候%1000==0概率很低
        // 1. 计算velodyne的时间戳
        long long veloTs = unixTime2DateTime(msg.header.stamp);
        // 2. 找到距离velodyne时间戳最近的GPS和IMU(dateTime, 单位ms)
        while ( gpsVec[countGps].ts > veloTs && countGps > 0) {
            -- countGps;
        }
        while ( gpsVec[countGps].ts < veloTs && countGps < gpsVec.size()-1) {
            ++ countGps;
        }
        while ( imuVec[countImu].ts > gpsVec[countGps].ts && countImu > 0) {
            -- countImu;
        }
        while ( imuVec[countImu].ts < gpsVec[countGps].ts && countImu < imuVec.size()-1) {
            ++ countImu;
        }
    }

    if (!this->hasGetSubscribe) {
        deltaUnixTs = msg.header.stamp.toNSec() - gpsVec[countGps].ts*1E6;
    }
    this->hasGetSubscribe = true;
}

}  // namespace server_node

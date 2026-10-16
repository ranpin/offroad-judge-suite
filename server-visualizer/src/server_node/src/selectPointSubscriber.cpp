/*****************************************************************************
** Includes
*****************************************************************************/

#include <ros/ros.h>
#include <ros/network.h>
#include <string>
#include <std_msgs/String.h>
#include <sstream>
#include "../include/server_node/selectPointSubscriber.h"

/*****************************************************************************
** Namespaces
*****************************************************************************/

namespace server_node {

/*****************************************************************************
** Implementation
*****************************************************************************/

selectPointSubscriber::selectPointSubscriber(int argc, char** argv, CConfig *config) :
    init_argc(argc),
    init_argv(argv),
    config(config)
    {}

selectPointSubscriber::~selectPointSubscriber() {
    if(ros::isStarted()) {
      ros::shutdown(); // explicitly needed since we use ros::start();
      ros::waitForShutdown();
    }
    wait();
}

bool selectPointSubscriber::init() {
    ros::init(init_argc,init_argv,"select_point_subscriber");
    if ( ! ros::master::check() ) {
        return false;
    }
    ros::start(); // explicitly needed since our nodehandle is going out of scope.
    this->sub_ = this->nh.subscribe("/clicked_point", 1, &selectPointSubscriber::pointCallback, this);
    start();
    return true;
}

void selectPointSubscriber::run() {
    // 需要一直spinOnce来保持callback可以被触发
    while ( this->nh.ok() ) {
        ros::spinOnce();
    }
    Q_EMIT rosShutdown(); // used to signal the gui for a shutdown (useful to roslaunch)
}

void selectPointSubscriber::pointCallback(const geometry_msgs::PointStamped & msg)
{
    // 接收到交互选择的坐标点后，将其转为经纬度并打印出来
    std::array<double, 2> selectPnt{-msg.point.y, msg.point.x};
    std::array<double, 2> wgs84Position{wgs84::fromCartesian(WGS84Reference, selectPnt)};

    std::array<double, 2> result{wgs84::toCartesian(WGS84Reference, wgs84Position)};

    double lbh[3], xyz[3];
    xyz[0] = msg.point.x; xyz[1] =  -msg.point.y; xyz[2] = msg.point.z;
    config->coordConvertor.LocalXYZ_GPS84LBH(xyz, lbh);
//    QString printInfo = QString("select point at: (%1, %2, %3)").arg(QString::number(wgs84Position[0], 'f', 7))
//                                                        .arg(QString::number(wgs84Position[1], 'f', 7))
//                                                        .arg(QString::number(msg.point.z, 'f', 7));
    QString printInfo = QString("select point at: (%1, %2, %3)").arg(QString::number(lbh[0], 'f', 7))
                                                        .arg(QString::number(lbh[1], 'f', 7))
                                                        .arg(QString::number(lbh[2], 'f', 7));
    qDebug() << printInfo;
    emit signalAppendText(printInfo);
}

}  // namespace server_node

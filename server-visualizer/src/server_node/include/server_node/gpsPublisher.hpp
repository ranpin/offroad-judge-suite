/**
 * @file /include/server_node/gpsPublisher.hpp
 *
 * @brief Communications central!
 *
 * @date February 2011
 **/
/*****************************************************************************
** Ifdefs
*****************************************************************************/

#ifndef server_node_gpsPublisher_HPP_
#define server_node_gpsPublisher_HPP_

/*****************************************************************************
** Includes
*****************************************************************************/

#ifndef Q_MOC_RUN
#include <ros/ros.h>
#endif
#include <string>
#include <QThread>
#include <QStringListModel>
#include <sensor_msgs/PointCloud2.h>
#include "cconfig.h"
#include "wgs84tocartesian.hpp"
#include "coordinateconvertion.h"
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/NavSatFix.h>
#include <nav_msgs/Odometry.h>
#include <tf/transform_broadcaster.h>


/*****************************************************************************
** Namespaces
*****************************************************************************/

namespace server_node {

/*****************************************************************************
** Class
*****************************************************************************/

class gpsPublisher : public QThread {
    Q_OBJECT
public:
  gpsPublisher(int argc, char** argv, Calib *veloCalib, CConfig *config);
  virtual ~gpsPublisher();
  bool init();
  void run();
  long long unixTime2DateTime(const ros::Time& stamp);
  void veloCallback(const sensor_msgs::PointCloud2& msg);
  QString getUnixTimeFromFileName(const QString &ts);

Q_SIGNALS:
    void rosShutdown();

private:
  int init_argc;
  char** init_argv;
  CConfig *config;
  Calib *veloCalib;
  ros::NodeHandle nh;
  ros::Publisher imuPub;
  ros::Publisher gpsPub;
  ros::Publisher odomPub;
  ros::Subscriber sub_;
  std::vector<GPSDATA> gpsVec;
  std::vector<IMUDATA> imuVec;
  int hasGetSubscribe = false;
  int countGps=0;          // 当前播放到了第几个数据条目
  int countImu=0;          // 当前播放到了第几个数据条目
  long long deltaUnixTs;     // 第一次收到激光数据时，记录它的unix时间戳和GPS dateTime的时间差(单位nsec)；发布GPS、IMU消息时（原始数据只有dateTime），根据这个数值推算出Unix时间戳
};

}  // namespace server_node

#endif /* server_node_gpsPublisher_HPP_ */

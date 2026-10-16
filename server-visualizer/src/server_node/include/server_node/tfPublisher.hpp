/**
 * @file /include/server_node/tfPublisher.hpp
 *
 * @brief Communications central!
 *
 * @date February 2011
 **/
/*****************************************************************************
** Ifdefs
*****************************************************************************/

#ifndef server_node_tfPublisher_HPP_
#define server_node_tfPublisher_HPP_

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

class tfPublisher : public QThread {
    Q_OBJECT
public:
  tfPublisher(int argc, char** argv, Calib *veloCalib, CConfig *config);
  virtual ~tfPublisher();
  bool init();
  void run();
  void odomCallback(const nav_msgs::Odometry &odomData);

Q_SIGNALS:
    void rosShutdown();

private:
  int init_argc;
  char** init_argv;
  CConfig *config;
  Calib *veloCalib;
  Calib gpsCalib;
  ros::NodeHandle nh;
  ros::Subscriber sub_;
  tf::TransformBroadcaster tfBroadcaster;
  tf::TransformBroadcaster lidarBboxTfBroadcaster;
  tf::TransformBroadcaster carTfBroadcaster;
  int countHistoryTf;
};

}  // namespace server_node

#endif /* server_node_tfPublisher_HPP_ */

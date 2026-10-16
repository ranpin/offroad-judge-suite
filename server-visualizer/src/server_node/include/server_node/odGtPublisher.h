/**
 * @file /include/server_node/odGtPublisher.h
 *
 * @brief Communications central!
 *
 * @date February 2011
 **/
/*****************************************************************************
** Ifdefs
*****************************************************************************/

#ifndef server_node_odGtPublisher_H_
#define server_node_odGtPublisher_H_

/*****************************************************************************
** Includes
*****************************************************************************/

#ifndef Q_MOC_RUN
#include <ros/ros.h>
#endif
#include <string>
#include <QThread>
#include <QStringListModel>
#include "cconfig.h"
#include "header.h"
#include "wgs84tocartesian.hpp"
#include "coordinateconvertion.h"
#include <nav_msgs/Odometry.h>
#include <sensor_msgs/PointCloud2.h>
#include <visualization_msgs/MarkerArray.h>


/*****************************************************************************
** Namespaces
*****************************************************************************/

namespace server_node {

/*****************************************************************************
** Class
*****************************************************************************/

// Object Detection Ground Truth
class odGtPublisher : public QThread {
    Q_OBJECT
public:
  odGtPublisher(int argc, char** argv, CConfig *config);
  virtual ~odGtPublisher();
  bool init();
  void run();
  long long unixTime2DateTime(const ros::Time& stamp);
  void odomCallback(const nav_msgs::Odometry &msg);
  void veloCallback(const sensor_msgs::PointCloud2& msg);
  void OD_gt_parser(const char* filename);

Q_SIGNALS:
    void rosShutdown();

public:
  int init_argc;
  char** init_argv;
  visualization_msgs::MarkerArray odGtMsg;
  visualization_msgs::MarkerArray odCenterGtMsg;
  CConfig *config;
  ros::NodeHandle nh;
  ros::Publisher odGtPub;
  ros::Publisher odCenterGtPub;
  ros::Subscriber sub_;
  ros::Subscriber odomSub_;
  std::vector<ObjDecGTRecord> odGtVec;
  nav_msgs::Odometry curOdom;
  int countOd = 0;
  int hasGetSubscribe = false;
};

}  // namespace server_node

#endif /* server_node_odGtPublisher_H_ */

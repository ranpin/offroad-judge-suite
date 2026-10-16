/**
 * @file /include/server_node/suGtPublisher.hpp
 *
 * @brief Communications central!
 *
 * @date February 2011
 **/
/*****************************************************************************
** Ifdefs
*****************************************************************************/

#ifndef server_node_suGtPublisher_HPP_
#define server_node_suGtPublisher_HPP_

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
#include <visualization_msgs/MarkerArray.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>


/*****************************************************************************
** Namespaces
*****************************************************************************/

namespace server_node {

/*****************************************************************************
** Class
*****************************************************************************/

// Scene Understanding Ground Truth
class suGtPublisher : public QThread {
    Q_OBJECT
public:
  suGtPublisher(int argc, char** argv, CConfig *config);
  virtual ~suGtPublisher();
  bool init();
  void run();
  long long unixTime2DateTime(const ros::Time& stamp);
  void odomCallback(const nav_msgs::Odometry& msg);

Q_SIGNALS:
    void rosShutdown();

private:
  int init_argc;
  char** init_argv;
  sensor_msgs::ImagePtr suImgMsg;
  visualization_msgs::MarkerArray locGtMsg;
  visualization_msgs::MarkerArray mapLocGtMsg;
  image_transport::ImageTransport* it;
  CConfig *config;
  ros::NodeHandle nh;
  ros::Publisher locGtPub;
  ros::Publisher mapLocGtPub;
  image_transport::Publisher imgPub;
  ros::Subscriber sub_;
  std::vector<GPSDATA> gpsVec;
  std::vector<GPSDATA> locGtVec;
  std::vector<GPSDATA> mapLocGtVec;
  std::vector<long long> suImgTsVec;
  nav_msgs::Odometry curOdom;
  int hasGetSubscribe = false;
  int countSuImg = -1;
};

}  // namespace server_node

#endif /* server_node_suGtPublisher_HPP_ */

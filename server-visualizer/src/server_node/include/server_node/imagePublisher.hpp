/**
 * @file /include/server_node/imagePublisher.hpp
 *
 * @brief Communications central!
 *
 * @date February 2011
 **/
/*****************************************************************************
** Ifdefs
*****************************************************************************/

#ifndef server_node_imagePublisher_HPP_
#define server_node_imagePublisher_HPP_

/*****************************************************************************
** Includes
*****************************************************************************/

// To workaround boost/qt4 problems that won't be bugfixed. Refer to
//    https://bugreports.qt.io/browse/QTBUG-22829
#ifndef Q_MOC_RUN
#include <ros/ros.h>
#endif
#include <string>
#include <QThread>
#include <QStringListModel>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <velodyne_msgs/VelodynePacket.h>
#include <velodyne_msgs/VelodyneScan.h>
#include <sensor_msgs/PointCloud2.h>
#include "header.h"
#include "odGtPublisher.h"


/*****************************************************************************
** Namespaces
*****************************************************************************/

namespace server_node {

/*****************************************************************************
** Class
*****************************************************************************/

class imagePublisher : public QThread {
    Q_OBJECT
public:
  imagePublisher(int argc, char** argv, std::vector<long long> &timeStampVec, std::vector<QString> &imgNameVec, odGtPublisher *odGtPub);
  virtual ~imagePublisher();
  bool init();
  void run();
  void veloCallback(const sensor_msgs::PointCloud2& msg);
  QString getUnixTimeFromFileName(const QString &ts);
  void drawBBox(cv::Mat &curImg);

Q_SIGNALS:
    void rosShutdown();

private:
  int init_argc;
  char** init_argv;
  sensor_msgs::ImagePtr msg;
  ros::NodeHandle nh;
  image_transport::ImageTransport* it;
  std::vector<long long> &timeStampVec;
  std::vector<QString> &imgNameVec;
  image_transport::Publisher pub;
  odGtPublisher *odGtPub;
  ros::Subscriber sub_;
  int countOd = 0;
  int hasGetSubscribe = false;
  int count=0;          // 当前播放到了第几个图片
};

}  // namespace server_node

#endif /* server_node_imagePublisher_HPP_ */

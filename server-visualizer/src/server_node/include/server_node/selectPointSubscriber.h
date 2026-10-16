/**
 * @file /include/server_node/selectPointSubscriber.h
 *
 * @brief Communications central!
 *
 * @date February 2011
 **/
/*****************************************************************************
** Ifdefs
*****************************************************************************/

#ifndef server_node_selectPointSubscriber_H_
#define server_node_selectPointSubscriber_H_

/*****************************************************************************
** Includes
*****************************************************************************/

#ifndef Q_MOC_RUN
#include <ros/ros.h>
#endif
#include <string>
#include <QThread>
#include <QStringListModel>
#include <geometry_msgs/PointStamped.h>
#include "wgs84tocartesian.hpp"
#include "coordinateconvertion.h"
#include "header.h"
#include "cconfig.h"

/*****************************************************************************
** Namespaces
*****************************************************************************/

namespace server_node {

/*****************************************************************************
** Class
*****************************************************************************/

class selectPointSubscriber : public QThread {
    Q_OBJECT
public:
  selectPointSubscriber(int argc, char** argv, CConfig *config);
  virtual ~selectPointSubscriber();
  bool init();
  void run();
  void pointCallback(const geometry_msgs::PointStamped & msg);

signals:
    void signalAppendText(const QString &text);

Q_SIGNALS:
    void rosShutdown();

private:
  int init_argc;
  char** init_argv;
  CConfig *config;
  ros::NodeHandle nh;
  ros::Subscriber sub_;
};

}  // namespace server_node

#endif /* server_node_selectPointSubscriber_H_ */

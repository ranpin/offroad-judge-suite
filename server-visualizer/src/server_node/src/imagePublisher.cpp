/**
 * @file /src/imagePublisher.cpp
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
#include "../include/server_node/imagePublisher.hpp"

/*****************************************************************************
** Namespaces
*****************************************************************************/

namespace server_node {

/*****************************************************************************
** Implementation
*****************************************************************************/

imagePublisher::imagePublisher(int argc, char** argv, std::vector<long long> &timeStampVec, std::vector<QString> &imgNameVec, odGtPublisher *odGtPub) :
	init_argc(argc),
    init_argv(argv),
    timeStampVec(timeStampVec),
    imgNameVec(imgNameVec),
    odGtPub(odGtPub)
	{}

imagePublisher::~imagePublisher() {
    if(ros::isStarted()) {
      ros::shutdown(); // explicitly needed since we use ros::start();
      ros::waitForShutdown();
    }
    wait();
}

bool imagePublisher::init() {
  count = 0;
  ros::init(init_argc,init_argv,"image_publisher");
	if ( ! ros::master::check() ) {
		return false;
	}
  ros::start(); // explicitly needed since our nodehandle is going out of scope.
  this->it = new image_transport::ImageTransport(this->nh);
  this->pub = this->it->advertise("camera/image", 1);
  this->sub_ = this->nh.subscribe("/velodyne_points", 1, &imagePublisher::veloCallback, this);
  start();
	return true;
}

void imagePublisher::drawBBox(cv::Mat &curImg)
{
    // 在图像上绘制目标检测框
    if (odGtPub == nullptr) return;
    long long imgTs = this->timeStampVec[this->count];
    // 在ObjectDetection的真值文件中,找有没有对应时间戳的bbox
    while (this->countOd < odGtPub->odGtVec.size()) {
        if (odGtPub->odGtVec.at(this->countOd).frame_timestamp.first > imgTs)
            break;
        // 当前帧中的目标,并且标注在Camera上
        if (odGtPub->odGtVec.at(this->countOd).frame_timestamp.first == imgTs
                && odGtPub->odGtVec.at(this->countOd).sensor_type=='C') {
            // 在图像上绘制包围框
            std::vector<cv::Point> bp;
            for (int i = 0; i < 4; ++ i) {
                cv::Point _p(odGtPub->odGtVec.at(this->countOd).bounding_box[i<<1], odGtPub->odGtVec.at(this->countOd).bounding_box[(i<<1)+1]);
                bp.push_back(_p);
            }
            // 根据目标类型绘制颜色
            cv::Scalar _c(255,255,255);
            std::string _text = "obj";
            switch (odGtPub->odGtVec.at(this->countOd).object_type) {
                case 'H': { // human
                    _c = cv::Scalar(255,0,0);
                    _text = "Human:" + std::to_string(odGtPub->odGtVec.at(this->countOd).object_idx)
                            + ":" + objStateStr[odGtPub->odGtVec.at(this->countOd).state].toStdString();
                    break;
                }
                case 'V': { // vehicle
                    _c = cv::Scalar(0,0,255);
                    _text = "Vehicle:" + std::to_string(odGtPub->odGtVec.at(this->countOd).object_idx)
                            + ":" + objStateStr[odGtPub->odGtVec.at(this->countOd).state].toStdString();
                    break;
                }
                case 'B': { // box
                    _c = cv::Scalar(0,255,0);
                    _text = "Box:" + std::to_string(odGtPub->odGtVec.at(this->countOd).object_idx)
                            + ":" + objStateStr[odGtPub->odGtVec.at(this->countOd).state].toStdString();
                    break;
                }
            }
            cv::polylines(curImg, bp, true, _c, 3);
            cv::putText(curImg, _text, cv::Point(bp[0].x, std::max(0, bp[0].y-10)), cv::FONT_HERSHEY_COMPLEX, 1.5, _c, 2);
        }
        ++ this->countOd;
    }
}

void imagePublisher::run() {
    // 相机数据帧率
    ros::Rate loop_rate(14.8);
    cv::Mat curImg;
    std_msgs::Header header;
    while ( this->nh.ok() ) {
        // 等待收到第一帧激光数据后，再开始播放视频
        if (hasGetSubscribe) {
            curImg = cv::imread(imgNameVec[count].toStdString().c_str());
            if (!curImg.empty()) {
                // 如果当前帧有对应的ObjectDetection Bounding Box,就画上去
                drawBBox(curImg);
                header.frame_id = 'camera';
                QString _tsStr = getUnixTimeFromFileName(imgNameVec[count]);
                ros::Time _ts;
                _ts.fromNSec(_tsStr.toLongLong()*1000);
                header.stamp = _ts;
                header.seq = count;
                this->msg = cv_bridge::CvImage(header, "bgr8", curImg).toImageMsg();
                pub.publish(msg);
            }
            if (count < imgNameVec.size()-1)
                ++count;
        }
        loop_rate.sleep();
        ros::spinOnce();
        }
    Q_EMIT rosShutdown(); // used to signal the gui for a shutdown (useful to roslaunch)
}

QString imagePublisher::getUnixTimeFromFileName(const QString &ts)
{
    QStringList strList = ts.split(QRegExp("[./]"));  // 分割出图像的unix时间戳的文件名
    return strList[strList.length()-2];
}

void imagePublisher::veloCallback(const sensor_msgs::PointCloud2& msg)
{
    this->hasGetSubscribe = true;
    // 每隔几秒校准一次图像和激光的时间,校准太频繁会卡顿
    if (count % 100 == 0) {  // 10秒
        // 1. 计算velodyne的时间戳
        QString veloTs = QString::number(msg.header.stamp.sec) + QString::number(int(msg.header.stamp.nsec/1000));
        QString imgTs;
        imgTs = getUnixTimeFromFileName(imgNameVec[count]);
        // 2. 找到距离velodyne时间戳最近的图像
        if (imgTs < veloTs) { // 如果图像播放慢了
          while (count < imgNameVec.size()-1) {
              imgTs = getUnixTimeFromFileName(imgNameVec[count]);
              if (imgTs < veloTs)
                  ++count;
              else
                  break;
          }
        }
        else { // 如果图像播放快了
            while (count > 0) {
                imgTs = getUnixTimeFromFileName(imgNameVec[count]);
                if (imgTs > veloTs)
                    --count;
                else
                    break;
            }
        }
    }
}

}  // namespace server_node

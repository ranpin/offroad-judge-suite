/**
 * @file /src/suGtPublisher.cpp
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
#include "../include/server_node/suGtPublisher.hpp"

/*****************************************************************************
** Namespaces
*****************************************************************************/

namespace server_node {

/*****************************************************************************
** Implementation
*****************************************************************************/

suGtPublisher::suGtPublisher(int argc, char** argv, CConfig *config):
	init_argc(argc),
    init_argv(argv),
    config(config)
	{}

suGtPublisher::~suGtPublisher() {
    if(ros::isStarted()) {
      ros::shutdown(); // explicitly needed since we use ros::start();
      ros::waitForShutdown();
    }
    wait();
}

bool cmp(const GPSDATA &a, const GPSDATA &b)
{
    return a.ts < b.ts;
}

bool suGtPublisher::init() {
    ros::init(init_argc,init_argv,"scene_understanding_gt_publisher");
    if ( ! ros::master::check() ) {
        return false;
    }
    ros::start(); // explicitly needed since our nodehandle is going out of scope.
    // 定义真值数据的消息发布(场景理解标注点的示例图片,以图像格式publish)
    this->it = new image_transport::ImageTransport(this->nh);
    this->imgPub = this->it->advertise("/gt/su_image", 1);
    // 定义真值数据的消息发布(定位考察点)
    this->locGtPub = this->nh.advertise<visualization_msgs::MarkerArray>("/gt/localization_point", 1);
    // 定义真值数据的消息发布(定位考察点)
    this->mapLocGtPub = this->nh.advertise<visualization_msgs::MarkerArray>("/gt/mapping_point", 1);
    // 监听odom数据
    this->sub_ = this->nh.subscribe("/odom", 1, &suGtPublisher::odomCallback, this);

    long long ts;
    double latitude, longitude, altitude;
    double NSV1,NSV2;  // 收星数，暂时没用
    // 1. 读取定位考察点的真值
    FILE *locf = std::fopen(config->gtLocalization[config->curDataset].toStdString().c_str(), "r");
    fscanf(locf, "%*[^\n]"); // 第一行标题，读入并忽略
    while (fscanf(locf, "%lld,%lf,%lf,%*lf\n", &ts, &latitude, &longitude) != EOF) {
        this->locGtVec.push_back(GPSDATA(ts, latitude, longitude, 0, 0, 0));
    }
    std::fclose(locf);
    sort(this->locGtVec.begin(), this->locGtVec.end(), cmp);

    // 2. 读取gps数据,找到定位考察点对应的高度(真值文件里没有高度数据)
    int scanfRet = 0;
    FILE *gpsf = std::fopen(config->gpsFile[config->curDataset].toStdString().c_str(), "r");
    fscanf(gpsf, "%*[^\n]");    // 第一行标题，读入并忽略
    scanfRet = fscanf(gpsf, "%lld,%lf,%lf,%lf,%lf,%lf\n", &ts, &latitude, &longitude, &altitude, &NSV1, &NSV2);
    while (scanfRet != EOF) {
        scanfRet = fscanf(gpsf, "%lld,%lf,%lf,%lf,%lf,%lf\n", &ts, &latitude, &longitude, &altitude, &NSV1, &NSV2);
        while (scanfRet == 0) { // 存在非法字符，跳过该数据条目
            fscanf(gpsf, "%*[^\n]");
            scanfRet = fscanf(gpsf, "%lld,%lf,%lf,%lf,%lf,%lf\n", &ts, &latitude, &longitude, &altitude, &NSV1, &NSV2);
        }
        gpsVec.push_back(GPSDATA(ts, latitude, longitude, altitude, NSV1, NSV2));
        // 查找该帧gps点,如果在定位真值中出现,则将高度赋值过去
        for (auto &loc_i : this->locGtVec)
            if (loc_i.ts == ts) {
                loc_i.altitude = altitude;
            }
    }
    std::fclose(gpsf);

    // 3. 读取场景理解真值图片,来自gt_mp.csv
    FILE *suf = std::fopen(config->gtSceneUnderstanding[config->curDataset].toStdString().c_str(), "r");
    fscanf(suf, "%*[^\n]");     // 第一行标题，读入并忽略
    while (fscanf(suf,  "%lld, %*[^\n]", &ts) != EOF) {
        suImgTsVec.push_back(ts);
    }
    sort(suImgTsVec.begin(), suImgTsVec.end());
    std::fclose(suf);

    // 4. 读取建图考核点,来自gt_global.csv
    FILE *ggf = std::fopen(config->gtGlobal[config->curDataset].toStdString().c_str(), "r");
    fscanf(ggf, "%*[^\n]");     // 第一行标题，读入并忽略
    while (fscanf(ggf,  "%lf, %lf, %*[^\n]", &latitude, &longitude) != EOF) {
        mapLocGtVec.push_back(GPSDATA(0, latitude, longitude, 0, 0, 0));
    }
    std::fclose(ggf);

    start();
    return true;
}

void suGtPublisher::run() {
    ros::Rate loop_rate(1);
    std_msgs::Header header;
    ros::Time _ts;
    while ( this->nh.ok() ) {
        // 等待收到第一帧Odom数据后，再开始播放GT数据
        if (this->hasGetSubscribe) {
            // 显示场景理解真值图像
            this->imgPub.publish(this->suImgMsg);
            // 显示定位考察点(一次性载入所有点,并发布)
            if (this->locGtMsg.markers.size() == 0) {   //第一次,需要构造所有定位点的marker
                for (int i = 0; i < this->locGtVec.size(); ++i) {
                    // 圆柱体(表示定位考察点)
                    visualization_msgs::Marker locMarker;
                    locMarker.header.frame_id = "/odom";
                    locMarker.header.stamp = this->suImgMsg->header.stamp;
                    locMarker.ns = "gt_localization_points";
                    locMarker.action = visualization_msgs::Marker::ADD;
                    locMarker.id = i;
                    locMarker.type = visualization_msgs::Marker::CYLINDER;
                    locMarker.scale.x = 1.5;
                    locMarker.scale.y = 1.5;
                    locMarker.scale.z = 10;
                    locMarker.color.b = 1.0;
                    locMarker.color.g = 0;
                    locMarker.color.r = 1.0;
                    locMarker.color.a = 0.2;

                    std::array<double, 2> WGS84Position{(this->locGtVec[i].latitude), (this->locGtVec[i].longitude)};
                    std::array<double, 2> result{wgs84::toCartesian(WGS84Reference, WGS84Position)};
                    double lbh[3], xyz[3];
                    lbh[0] = this->locGtVec[i].latitude; lbh[1] = this->locGtVec[i].longitude; lbh[2] = this->locGtVec[i].altitude;
                    config->coordConvertor.GPS84LBH_LocalXYZ(lbh, xyz);

//                    locMarker.pose.position.x = result[1];
//                    locMarker.pose.position.y = -result[0];
                    locMarker.pose.position.x = xyz[0];
                    locMarker.pose.position.y = -xyz[1];
                    locMarker.pose.position.z = this->locGtVec[i].altitude+2;
                    locMarker.pose.orientation.w = 1.0;
                    this->locGtMsg.markers.push_back(locMarker);

                    // 文字
                    visualization_msgs::Marker locTextMarker;
                    locTextMarker.header.frame_id = "/odom";
                    locTextMarker.header.stamp = this->suImgMsg->header.stamp;
                    locTextMarker.ns = "gt_localization_points";
                    locTextMarker.action = visualization_msgs::Marker::ADD;
                    locTextMarker.id = i+this->locGtVec.size();
                    locTextMarker.type = visualization_msgs::Marker::TEXT_VIEW_FACING;
                    locTextMarker.scale.z = 2;
                    locTextMarker.color.b = 1.0;
                    locTextMarker.color.g = 1.0;
                    locTextMarker.color.r = 1.0;
                    locTextMarker.color.a = 0.8;
                    locTextMarker.pose.position.x = locMarker.pose.position.x;
                    locTextMarker.pose.position.y = locMarker.pose.position.y;
                    locTextMarker.pose.position.z = locMarker.pose.position.z+4;
                    locTextMarker.pose.orientation.w = 1.0;
                    locTextMarker.text = QString("Loc:%1").arg(i).toStdString();
                    this->locGtMsg.markers.push_back(locTextMarker);
                }
            }
            else {  // 后续只更新时间戳
                for (int i = 0; i < this->locGtMsg.markers.size(); ++i) {
                    this->locGtMsg.markers.at(i).header.stamp = this->suImgMsg->header.stamp;
                }
            }
            this->locGtPub.publish(this->locGtMsg);

            // 显示建图考察点(一次性载入所有点,并发布)
            if (this->mapLocGtMsg.markers.size() == 0) {   //第一次,需要构造所有定位点的marker
                for (int i = 0; i < this->mapLocGtVec.size(); ++i) {
                    // 圆柱体(表示建图考察点)
                    visualization_msgs::Marker mapLocMarker;
                    mapLocMarker.header.frame_id = "/odom";
                    mapLocMarker.header.stamp = this->suImgMsg->header.stamp;
                    mapLocMarker.ns = "gt_mapping_points";
                    mapLocMarker.action = visualization_msgs::Marker::ADD;
                    mapLocMarker.id = i;
                    mapLocMarker.type = visualization_msgs::Marker::CYLINDER;
                    mapLocMarker.scale.x = 2.0;
                    mapLocMarker.scale.y = 2.0;
                    mapLocMarker.scale.z = 10;
                    mapLocMarker.color.b = 0.0;
                    mapLocMarker.color.g = 0.0;
                    mapLocMarker.color.r = 1.0;
                    mapLocMarker.color.a = 0.7;

                    std::array<double, 2> WGS84Position{(this->mapLocGtVec[i].latitude), (this->mapLocGtVec[i].longitude)};
                    std::array<double, 2> result{wgs84::toCartesian(WGS84Reference, WGS84Position)};
                    std::array<double, 2> wgs84Position{wgs84::fromCartesian(WGS84Reference, result)};
                    double lbh[3], xyz[3];
                    lbh[0] = this->mapLocGtVec[i].latitude; lbh[1] = this->mapLocGtVec[i].longitude; lbh[2] = this->curOdom.pose.pose.position.z;
                    config->coordConvertor.GPS84LBH_LocalXYZ(lbh, xyz);

//                    mapLocMarker.pose.position.x = result[1];
//                    mapLocMarker.pose.position.y = -result[0];
                    mapLocMarker.pose.position.x = xyz[0];
                    mapLocMarker.pose.position.y = -xyz[1];
                    mapLocMarker.pose.position.z = this->curOdom.pose.pose.position.z+2;
                    mapLocMarker.pose.orientation.w = 1.0;
                    this->mapLocGtMsg.markers.push_back(mapLocMarker);

                    // 文字
                    visualization_msgs::Marker locTextMarker;
                    locTextMarker.header.frame_id = "/odom";
                    locTextMarker.header.stamp = this->suImgMsg->header.stamp;
                    locTextMarker.ns = "gt_mapping_points";
                    locTextMarker.action = visualization_msgs::Marker::ADD;
                    locTextMarker.id = i+this->mapLocGtVec.size();
                    locTextMarker.type = visualization_msgs::Marker::TEXT_VIEW_FACING;
                    locTextMarker.scale.z = 2;
                    locTextMarker.color.b = 1.0;
                    locTextMarker.color.g = 1.0;
                    locTextMarker.color.r = 1.0;
                    locTextMarker.color.a = 0.8;
                    locTextMarker.pose.position.x = mapLocMarker.pose.position.x;
                    locTextMarker.pose.position.y = mapLocMarker.pose.position.y;
                    locTextMarker.pose.position.z = mapLocMarker.pose.position.z+5;
                    locTextMarker.pose.orientation.w = 1.0;
                    locTextMarker.text = QString("GL:%1").arg(i).toStdString();
                    this->mapLocGtMsg.markers.push_back(locTextMarker);
                }
            }
            else {  // 后续只更新时间戳和高度
                for (int i = 0; i < this->mapLocGtMsg.markers.size(); ++i) {
                    this->mapLocGtMsg.markers.at(i).header.stamp = this->suImgMsg->header.stamp;
                    if (this->mapLocGtMsg.markers.at(i).type == visualization_msgs::Marker::CYLINDER)
                        this->mapLocGtMsg.markers.at(i).pose.position.z = this->curOdom.pose.pose.position.z+2;
                    else
                        this->mapLocGtMsg.markers.at(i).pose.position.z = this->curOdom.pose.pose.position.z+5;
                }
            }
            this->mapLocGtPub.publish(this->mapLocGtMsg);
        }
        loop_rate.sleep();
        ros::spinOnce();
    }
    Q_EMIT rosShutdown(); // used to signal the gui for a shutdown (useful to roslaunch)
}

long long suGtPublisher::unixTime2DateTime(const ros::Time& stamp)
{
    // 输入的unix时间戳，输出timeofday时间戳，单位毫秒
    std::time_t rawTime = static_cast<time_t>(stamp.sec);
    struct tm* timeInfo = localtime(&rawTime);
    long long dateTime = timeInfo->tm_hour*3600+timeInfo->tm_min*60+timeInfo->tm_sec;
    dateTime = dateTime * 1000 + stamp.nsec / 1E6;
    return dateTime;
}

void suGtPublisher::odomCallback(const nav_msgs::Odometry &msg)
{
    // 监听Odom的topic，根据其时间戳更新要发布的场景理解真值图片
    this->curOdom = msg;
    // 1. 计算dateTime时间戳
    long long odomTs = unixTime2DateTime(msg.header.stamp);
    long long nextSuImgTs = this->suImgTsVec[std::min(countSuImg+1, int(this->suImgTsVec.size()-1))];
    // 2. 找到odom时间戳下,最新的真值图像
    if ((nextSuImgTs <= odomTs && countSuImg < this->suImgTsVec.size()-1) || (countSuImg < 0)) { // 需要更新真值图像
        ++ countSuImg;
        QString imgTs = QString::number(this->suImgTsVec[countSuImg]);
        // 3. 读取需要显示的真值图像
        QString imgName = config->gtGlobalPng[config->curDataset];
        cv::Mat curSuImg = cv::imread(imgName.replace(QString("global"), imgTs).toStdString().c_str());
        std_msgs::Header header;
        header.frame_id = 'camera';
        header.stamp = msg.header.stamp;
        header.seq = countSuImg;
        this->suImgMsg = cv_bridge::CvImage(header, "bgr8", curSuImg).toImageMsg();
    }
    else if (this->hasGetSubscribe) {  // 不更新图像,只更新时间戳
        this->suImgMsg->header.stamp = msg.header.stamp;
    }
    // 3. 找到当前odom时间附近的定位点,将可视化marker颜色变深
    for (int i = 0; i < this->locGtMsg.markers.size(); ++i) {
        int idx = this->locGtMsg.markers.at(i).id;
        if (idx >= this->locGtVec.size()) continue;
        // 判断时间戳是否在odom时间附近(提前2000ms)
        if (odomTs - this->locGtVec[idx].ts > -2000) {
            this->locGtMsg.markers.at(i).color.a = 0.65;
        }
    }

    this->hasGetSubscribe = true;
}

}  // namespace server_node

/**
 * @file /src/odGtPublisher.cpp
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
#include <cstdio>
#include <iostream>
#include "../include/server_node/odGtPublisher.h"

/*****************************************************************************
** Namespaces
*****************************************************************************/

namespace server_node {

/*****************************************************************************
** Implementation
*****************************************************************************/

odGtPublisher::odGtPublisher(int argc, char** argv, CConfig *config):
	init_argc(argc),
    init_argv(argv),
    config(config)
	{}

odGtPublisher::~odGtPublisher() {
    if(ros::isStarted()) {
      ros::shutdown(); // explicitly needed since we use ros::start();
      ros::waitForShutdown();
    }
    wait();
}

bool cmp(const ObjDecGTRecord &a, const ObjDecGTRecord &b)
{
    return a.frame_timestamp.first < b.frame_timestamp.first;
}

bool odGtPublisher::init() {
    ros::init(init_argc,init_argv,"object_detection_gt_publisher");
    if ( ! ros::master::check() ) {
        return false;
    }
    ros::start(); // explicitly needed since our nodehandle is going out of scope.
    // 定义真值数据的消息发布(3D bounding box)
    this->odGtPub = this->nh.advertise<visualization_msgs::MarkerArray>("/gt/bounding_box", 1);
    // 定义真值数据的消息发布(object center)
    this->odCenterGtPub = this->nh.advertise<visualization_msgs::MarkerArray>("/gt/object_center", 1);
    // 监听velodyne数据
    this->sub_ = this->nh.subscribe("/velodyne_points", 1, &odGtPublisher::veloCallback, this);
    // 监听odom数据
    this->odomSub_ = this->nh.subscribe("/odom", 1, &odGtPublisher::odomCallback, this);

    // 1. 读取Bounding Box的真值
    OD_gt_parser(config->gtDetection[config->curDataset].toStdString().c_str());
    std::sort(this->odGtVec.begin(), this->odGtVec.end(), cmp);
    // 2. 判断是否有Bounding Box属性文件，如果有则读入obj属性信息
    FILE* fStat = std::fopen(config->gtDetection[config->curDataset].replace(QString("gt_detection.csv"), QString("state.txt")).toStdString().c_str(), "r");
    std::map<int, int> objStatMap;
    if (fStat != NULL) {
        int objID, objStat;
        while (std::fscanf(fStat, "%d %d\n", &objID, &objStat) != EOF) {
            objStatMap[objID] = objStat;
        }
        for (int i = 0; i < this->odGtVec.size(); ++ i) {
            this->odGtVec.at(i).state = objStatMap[this->odGtVec.at(i).object_idx];
        }
        std::fclose(fStat);
    }
    start();
    return true;
}

void odGtPublisher::run() {}

long long odGtPublisher::unixTime2DateTime(const ros::Time& stamp)
{
    // 输入的unix时间戳，输出timeofday时间戳，单位毫秒
    std::time_t rawTime = static_cast<time_t>(stamp.sec);
    struct tm* timeInfo = localtime(&rawTime);
    long long dateTime = timeInfo->tm_hour*3600+timeInfo->tm_min*60+timeInfo->tm_sec;
    dateTime = dateTime * 1000 + stamp.nsec / 1E6;
    return dateTime;
}

void odGtPublisher::odomCallback(const nav_msgs::Odometry &msg)
{
    this->curOdom = msg;
}

void odGtPublisher::veloCallback(const sensor_msgs::PointCloud2 &msg)
{
    this->hasGetSubscribe = true;
    // 遍历Object Detection GT,找到和当前velodyne帧匹配的bounding box
    long long veloTs = unixTime2DateTime(msg.header.stamp);
    int countMarker = 0;
    this->odGtMsg.markers.clear();
    this->odCenterGtMsg.markers.clear();
    while (countOd < odGtVec.size()) {
        if (odGtVec.at(countOd).frame_timestamp.first > veloTs)
            break;
        // 如果找到了符合条件的bbox,可视化出来
        if (odGtVec.at(countOd).sensor_type == 'L' &&
            veloTs >= odGtVec.at(countOd).frame_timestamp.first && veloTs <= odGtVec.at(countOd).frame_timestamp.second) {
            float _b = 0, _g = 0, _r = 0;
            //根据目标类型估计3D Box高度,并设置可视化颜色
            double _h = 2.0; // 目标物体的高度
            switch (odGtVec.at(countOd).object_type) {
                case 'H':
                    _h = 2.0; _b = 1; break;
                case 'V':
                    _h = 2.0; _r = 1; break;
                case 'B':
                    _h = 2.0; _g = 1; break;
            }
            // 包围框
            visualization_msgs::Marker bboxMarker;
            bboxMarker.header.frame_id = "/lidar_bbox";
            bboxMarker.header.stamp = msg.header.stamp;
            bboxMarker.ns = "gt_object_detection";
            bboxMarker.action = visualization_msgs::Marker::ADD;
            bboxMarker.id = countMarker ++;
            bboxMarker.type = visualization_msgs::Marker::LINE_LIST;
            bboxMarker.scale.x = 0.2;
            bboxMarker.color.b = _b;
            bboxMarker.color.g = _g;
            bboxMarker.color.r = _r;
            bboxMarker.color.a = 1;
            bboxMarker.lifetime = ros::Duration(0.1);

            bboxMarker.pose.position.x = 0;
            bboxMarker.pose.position.y = 0;
            bboxMarker.pose.position.z = 0;
            bboxMarker.pose.orientation.w = 1.0;

            // 遍历顶点,画出包围框的每根线(根据目标类型设置一个假的包围框高度,因为真值里面的包围框是俯视图的二维框)

            geometry_msgs::Point p;
            // 1. 先画底部框
            for (int i = 0; i < 4; ++ i) {
                p.x = odGtVec.at(countOd).bounding_box[i*2];
                p.y = odGtVec.at(countOd).bounding_box[i*2+1];
                p.z = -0.5;
                bboxMarker.points.push_back(p);
                p.x = odGtVec.at(countOd).bounding_box[(i*2+2) % odGtVec.at(countOd).bounding_box.size()];
                p.y = odGtVec.at(countOd).bounding_box[(i*2+3) % odGtVec.at(countOd).bounding_box.size()];
                p.z = -0.5;
                bboxMarker.points.push_back(p);
            }
            // 2. 画顶部框
            for (int i = 0; i < 4; ++ i) {
                p.x = odGtVec.at(countOd).bounding_box[i*2];
                p.y = odGtVec.at(countOd).bounding_box[i*2+1];
                p.z = _h;
                bboxMarker.points.push_back(p);
                p.x = odGtVec.at(countOd).bounding_box[(i*2+2) % odGtVec.at(countOd).bounding_box.size()];
                p.y = odGtVec.at(countOd).bounding_box[(i*2+3) % odGtVec.at(countOd).bounding_box.size()];
                p.z = _h;
                bboxMarker.points.push_back(p);
            }
            // 3. 画顶部框和底部框的连线,组成立方体
            for (int i = 0; i < 4; ++ i) {
                p.x = odGtVec.at(countOd).bounding_box[i*2];
                p.y = odGtVec.at(countOd).bounding_box[i*2+1];
                p.z = -0.5;
                bboxMarker.points.push_back(p);
                p.x = odGtVec.at(countOd).bounding_box[i*2];
                p.y = odGtVec.at(countOd).bounding_box[i*2+1];
                p.z = _h;
                bboxMarker.points.push_back(p);
            }
            // 将所有点放入MarkerList
            this->odGtMsg.markers.push_back(bboxMarker);

            //  画出目标中心点(从经纬度转换过来)
            visualization_msgs::Marker obCenterMarker;
            obCenterMarker.header.frame_id = "/odom";
            obCenterMarker.header.stamp = msg.header.stamp;
            obCenterMarker.ns = "gt_object_detection";
            obCenterMarker.action = visualization_msgs::Marker::ADD;
            obCenterMarker.id = countMarker ++;
            obCenterMarker.type = visualization_msgs::Marker::SPHERE;
            obCenterMarker.scale.x = 0.8;
            obCenterMarker.scale.y = 0.8;
            obCenterMarker.scale.z = 0.8;
            obCenterMarker.color.b = 0;
            obCenterMarker.color.g = 1.0;
            obCenterMarker.color.r = 1.0;
            obCenterMarker.color.a = 1;
            obCenterMarker.lifetime = ros::Duration(0.1);

            std::array<double, 2> WGS84Position{(odGtVec.at(countOd).gps_loc.first), (odGtVec.at(countOd).gps_loc.second)};
            std::array<double, 2> result{wgs84::toCartesian(WGS84Reference, WGS84Position)};

            double lbh[3], xyz[3];
            lbh[0] = odGtVec.at(countOd).gps_loc.first; lbh[1] = odGtVec.at(countOd).gps_loc.second; lbh[2] = this->curOdom.pose.pose.position.z;
            config->coordConvertor.GPS84LBH_LocalXYZ(lbh, xyz);

//            obCenterMarker.pose.position.x = result[1];
//            obCenterMarker.pose.position.y = -result[0];
            obCenterMarker.pose.position.x = xyz[0];
            obCenterMarker.pose.position.y = -xyz[1];
            obCenterMarker.pose.position.z = this->curOdom.pose.pose.position.z+_h;   // 根据当前车辆高度可视化目标高度
            obCenterMarker.pose.orientation.w = 1.0;
            this->odCenterGtMsg.markers.push_back(obCenterMarker);

            // 文字说明
            visualization_msgs::Marker bboxTextMarker;
            bboxTextMarker.header.frame_id = "/lidar_bbox";
            bboxTextMarker.header.stamp = msg.header.stamp;
            bboxTextMarker.ns = "gt_object_detection";
            bboxTextMarker.action = visualization_msgs::Marker::ADD;
            bboxTextMarker.id = countMarker ++;
            bboxTextMarker.type = visualization_msgs::Marker::TEXT_VIEW_FACING;
            bboxTextMarker.scale.z = 2;
            bboxTextMarker.color.b = _b;
            bboxTextMarker.color.g = _g;
            bboxTextMarker.color.r = _r;
            bboxTextMarker.color.a = 1;
            bboxTextMarker.lifetime = ros::Duration(0.1);
            bboxTextMarker.pose.position.x = p.x;
            bboxTextMarker.pose.position.y = p.y;
            bboxTextMarker.pose.position.z = bboxMarker.pose.position.z+3;
            bboxTextMarker.pose.orientation.w = 1.0;
            bboxTextMarker.text = QString("%1:%2:%3").arg(odGtVec.at(countOd).object_type)
                                                     .arg(odGtVec.at(countOd).object_idx)
                                                     .arg(objStateStr[odGtVec.at(countOd).state]).toStdString();
            this->odGtMsg.markers.push_back(bboxTextMarker);
            qDebug() << QString("Object: %1").arg(odGtVec.at(countOd).object_idx);
        }
        ++ countOd;
    }
    odGtPub.publish(odGtMsg);
    odCenterGtPub.publish(odCenterGtMsg);
}

void odGtPublisher::OD_gt_parser(const char* filename)
{
    std::ifstream file;
    file.open(filename);
    if (!file.good())
    {
        qDebug() <<"Cannot open "<<filename<<", please put the file in the same path with program, exit program...\n";
        exit(-2);
    }
    qDebug() <<"Open "<<filename<<" successfully!\n";
    char skipped_char; // 用于保存逗号
    int skipped_int; // 用于保存整数

    int idx;
    std::pair<long long int, long long int> frame_timestamp_;
    char sensor_type_, obj_type_;

    double double_tmp;
    std::pair<double, double> gps_loc_;
    int object_idx_;
    std::pair<long long int, long long int> life_timestamp_;
    while(!file.eof())
    {
        file >> idx;
        if(file.eof())
            break;
        file >> skipped_char;
        if(file.eof())
            break;
        file >> frame_timestamp_.first;
        file >> skipped_char;
        file >> frame_timestamp_.second;
        file >> skipped_char;
        // 读入包围框类型
        file >> sensor_type_;
        file >> skipped_char;
        // 读入目标属性
        file >> obj_type_;
        file >> skipped_char;
        file >> skipped_char;
        file >> skipped_char;
        std::vector<double> box;
        // 读入包围框顶点坐标
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 2; j++) {
                file >> double_tmp;
                file >> skipped_char;
                box.push_back(double_tmp);
            }
        }
        file >> skipped_char;
        file >> skipped_char;
        file >> skipped_char;
        file >> skipped_char;

        // 读取目标位置精度
        file >> gps_loc_.first;

        file >> skipped_char;

        // 读取目标位置纬度
        file >> gps_loc_.second;

        file >> skipped_char;
        file >> skipped_char;
        file >> skipped_char;
        file >> object_idx_;
        file >> skipped_char;
        file >> life_timestamp_.first;
        file >> skipped_char;
        file >> life_timestamp_.second;

        ObjDecGTRecord newrec(idx,frame_timestamp_,sensor_type_,obj_type_,box,gps_loc_,object_idx_,life_timestamp_,0);
        this->odGtVec.push_back(newrec);

    }
    file.close();
}
}  // namespace server_node

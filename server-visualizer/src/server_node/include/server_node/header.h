#ifndef CHEADER_H
#define CHEADER_H
/*
 * @Author: challenge-tech-team
 * @Date: 2021-07-10 21:26:51
 * @Description: information from config file
 */

#include <cstring>
#include <cmath>
#include <algorithm>
#include <memory>
#include <iostream>
#include <fstream>
#include <QtNetwork/QHostAddress>
#include <QtWidgets/QTextBrowser>
#include <QtNetwork/QTcpServer>
#include <QThreadPool>
#include <QDebug>
#include <QtNetwork/QTcpSocket>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QProgressBar>
#include <QTimer>
#include <QProcess>
#include <QDir>
#include <QMetaType>
#include <QDateTime>
#include <QtWidgets/QLabel>
#include <opencv2/opencv.hpp>
#include "tf/tf.h"
#include "coordinateconvertion.h"
#include "main_window.hpp"
#include "ui_main_window.h"

// 字符    ASCII码      令牌类型
//  C       0x43        单目相机
//  G       0x47        CANBUS 档位
//  O       0x4F        CANBUS 编码器
//  S       0x53        CANBUS 方向盘
//  P       0x50        GPS
//  U       0x55        惯导
//  L       0x4C        多线激光雷达
//  R       0x52        单线激光雷达

const int TASK_PREPARE_COMPETITION = 0;
const int TASK_START_COMPETITION = 1;
const int TASK_SEND_DATA_TOKEN = 2;
const int TASK_END_COMPETITION = 3;

// 比赛科目
const int SCENE_UNDERSTANDING = 0;
const int OBJECT_DETECTION = 1;

// Prepare Competition: 0x023003
// Competition Ready: 0x023203
// Competition Over: 0x023303
static const char cmdPrepareCompetition[] = {0x02, 0x30, 0x03};
static const char cmdStartCompetition[] = {0x02, 0x31, 0x03};
static const char cmdCompetitionReady[] = {0x02, 0x32, 0x03};
static const char cmdCompetitionOver[] = {0x02, 0x33, 0x03};
static const char cmdAnswerOver[] = {0x02, 0x35, 0x03};
static const unsigned char dataTypes[] = {'C','G','O','S','P','U','L','R'};

constexpr std::array<double, 2> WGS84Reference{40.88379176, 115.8807585};      // Beijing

// ObjDetection物体状态
const QString objStateStr[6] = {"Normal", "Standing", "Moving", "Throwing", "Crouching", "Static"};

// 车队的信息
struct CPlayer {
    unsigned int id;
    std::string name;
    std::string ip;
    CPlayer(){}
    CPlayer(unsigned int id, std::string name, std::string ip):id(id),name(name),ip(ip){}
};

// 车队信息和考试准备状态
//struct CPlayerState
//{
//    CPlayer info;
//    bool isReady;    // 0: not ready   1: ready for competition
//};

// 一个data token (时间戳+数据类型+数据令牌)
struct CDataToken
{
    long long timeStamp;
    uchar type; // 数据类型
    uchar token;
    CDataToken(long long ts, uchar _type, uchar _token):timeStamp(ts),type(_type),token(_token) {}
};

struct Calib
{
    double rot[3];  // roll, pitch, yaw
    double shv[3];  // x, y, z
};

struct GPSDATA{
    long long ts;
    double latitude, longitude, altitude;
    double NSV1,NSV2;  // 收星数，暂时没用
    GPSDATA(long long ts, double lat, double longi, double alti, double nsv1, double nsv2):
        ts(ts),latitude(lat),longitude(longi),altitude(alti),NSV1(nsv1),NSV2(nsv2){};
};

struct IMUDATA{
    long long ts;
    double heading, pitch, roll;
    double vEast, vNorth, vUp;
    IMUDATA(long long ts, double heading, double pitch, double roll, double vEast, double vNorth, double vUp):
        ts(ts),heading(heading),pitch(pitch),roll(roll),vEast(vEast),vNorth(vNorth),vUp(vUp){};
};

struct ObjDecGTRecord
{
    int idx;
    std::pair <long long, long long> frame_timestamp;
    char sensor_type;
    char object_type;
    std::vector<double> bounding_box;
    std::pair <double,double> gps_loc;
    int object_idx;
    std::pair <long long, long long> life_timestamp;
    int state;
    // construction function
    ObjDecGTRecord(){};
    ObjDecGTRecord(int idx_, std::pair<long long int, long long int> frame_timestamp_, char sensor_type_,
                   char object_type_, const std::vector<double>& bounding_box_, std::pair<double,double> gps_loc_,
                   int object_idx_, std::pair<long long int, long long int> life_timestamp_, int state_):
        idx(idx_),
        frame_timestamp(frame_timestamp_),
        sensor_type(sensor_type_),
        object_type(object_type_),
        bounding_box(bounding_box_),
        gps_loc(gps_loc_),
        object_idx(object_idx_),
        life_timestamp(life_timestamp_),
        state(state_){};
};

#endif // CHEADER_H

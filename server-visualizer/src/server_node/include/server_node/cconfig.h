#ifndef CCONFIG_H
#define CCONFIG_H

#include "header.h"

// 读取并保存ini配置文件的信息
class CConfig {
public:
    CConfig();
public:
    void showMessageBox(const QString &text);
    quint16 requestListenPort = 6000;     // 考试调度指令的监听端口
    quint16 scoreListenPort = 6003;       // 得分回传的监听端口
    quint16 clientCommandPort = 6001;     // 客户端处接受考试指令的端口
    quint16 clientTokenPort = 6002;       // 客户端处接受数据令牌的端口
    int waitTime = 100000;               // Wait time for TCP connection response
    std::vector<int> competitionType;            // 0 环境建模 1 目标探测
    QStringList tokenFile;              // 数据令牌的存储文件
    QStringList dataPath;               // 数据文件夹
    QStringList timeStampFile;          // camera时间戳的存储文件
    QStringList veloFile;               // velodyne的pcap文件
    QStringList cameraFolder;           // camera文件夹的路径
    QStringList veloCalib;              // velodyne标定文件
    QStringList gpsCalib;               // gps到车体的标定文件
    QStringList gtDetection;            // 目标检测真值文件
    QStringList gtGlobalPng;            // 环境建模中全局地图的任务文件global.png
    QStringList gtLocalization;         // 环境建模中定位考核点真值文件gt_local.csv
    QStringList gtGlobal;               // 环境建模中gt_global.csv(建图的考核点)
    QStringList gtSceneUnderstanding;   // 场景理解真值文件列表gt_map.csv
    QStringList gpsFile;                // gps文件
    QStringList imuFile;                // IMU文件
    std::string outputScoreFile;        // 统计完得分保存成文件
    std::string playerInfoFile;        // 统计完得分保存成文件
    std::vector<CPlayer> playerVec;     // 车队信息
    CoordinateConvertion coordConvertor;
    int curDataset = 0;                 // 如果连续进行多个考试，这是当前考试的编号，根据这个到各种数据文件的QStringList中找到对应的文件名
};


#endif // CCONFIG_H

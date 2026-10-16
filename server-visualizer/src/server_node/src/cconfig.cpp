#include "../include/server_node/cconfig.h"

using namespace  std;

CConfig::CConfig()
{
    // Read config file.
    cv::FileStorage fs("config.yml", cv::FileStorage::READ);
    if(fs.isOpened())
        std::cout<<"Load config.yml successfully!"<<std::endl;
    else
    {
        showMessageBox(QString("Cannot open config.yml in working space!"));
        std::cout<<"Cannot open config.yml, exit program... "<<std::endl;
        QMessageBox msgBox;
        msgBox.setText("Cannot open config.yml");
        msgBox.exec();
        exit(-1);
    }
    fs["requestListenPort"] >> requestListenPort;
    fs["scoreListenPort"] >> scoreListenPort;
    fs["clientCommandPort"] >> clientCommandPort;
    fs["clientTokenPort"] >> clientTokenPort;
    fs["tcpWaitTime"] >> waitTime;
    std::string allDataPath;
    fs["dataPath"] >> allDataPath;
    // 所有的数据文件夹路径按照分号分割
    this->dataPath = QString::fromStdString(allDataPath).split(';');
    fs["outputScoreFile"] >> outputScoreFile;
    fs["playerInfoFile"] >> playerInfoFile;

    // Find file path of different sensors
    for (QString _dataPath : dataPath) {
        QDir dirPath(_dataPath);
        qDebug() << "Opening dataset "+_dataPath;
        assert(dirPath.exists());
        // 1. velodyne
        dirPath.setNameFilters(QStringList(QString("*.pcap")));
        if (dirPath.entryInfoList().size() <= 0) {
            qDebug() << "Can't find *.pcap\n";
            showMessageBox(QString("Cannot open *.pcap!"));
        }
        else {
            this->veloFile.push_back(dirPath.entryInfoList()[0].absoluteFilePath());
        }
        // 2. camera time stamp
        dirPath.setNameFilters(QStringList(QString("*Camera-Timestamp.csv")));
        if (dirPath.entryInfoList().size() <= 0) {
            qDebug() << "Can't find *Camera-Timestamp.csv\n";
            showMessageBox(QString("Cannot open *Camera-Timestamp.csv!"));
        }
        else {
            this->timeStampFile.push_back(dirPath.entryInfoList()[0].absoluteFilePath());
        }
        // 3. camera folder
        dirPath.setNameFilters(QStringList(QString("*-Camera")));
        if (dirPath.entryInfoList().size() <= 0) {
            qDebug() << "Can't find *Camera\n";
            showMessageBox(QString("Cannot open *Camera!"));
        }
        else {
            this->cameraFolder.push_back(dirPath.entryInfoList()[0].absoluteFilePath());
        }
        // 4.1 velodyne calib file
        dirPath.setNameFilters(QStringList(QString("*-Calib")));
        QDir calibPath(dirPath.entryInfoList()[0].absoluteFilePath());
        calibPath.setNameFilters(QStringList(QString("*-HDL32-E.txt")));
        if (calibPath.entryInfoList().size() <= 0) {
            qDebug() << "Can't find *-Calib/*-HDL32-E.txt\n";
            showMessageBox(QString("Cannot open *-Calib/*-HDL32-E.txt!"));
        }
        else {
            this->veloCalib.push_back(calibPath.entryInfoList()[0].absoluteFilePath());
        }
        // 4.2 gps calib file
        calibPath.setNameFilters(QStringList(QString("*-GPS.txt")));
        if (calibPath.entryInfoList().size() <= 0) {
            qDebug() << "Can't find *-Calib/*-GPS.txt\n";
            showMessageBox(QString("Cannot open *-Calib/*-GPS.txt!"));
        }
        else {
            this->gpsCalib.push_back(calibPath.entryInfoList()[0].absoluteFilePath());
        }
        // 5. token file
        dirPath.setNameFilters(QStringList(QString("token.csv")));
        if (dirPath.entryInfoList().size() <= 0) {
            qDebug() << "Can't find token.csv\n";
            showMessageBox(QString("Cannot open token.csv!"));
        }
        else {
            this->tokenFile.push_back(dirPath.entryInfoList()[0].absoluteFilePath());
        }
        // 6. gps file
        dirPath.setNameFilters(QStringList(QString("*-GPS.csv")));
        if (dirPath.entryInfoList().size() <= 0) {
            qDebug() << "Can't find *-GPS.csv\n";
            showMessageBox(QString("Cannot open *-GPS.csv!"));
        }
        else {
            this->gpsFile.push_back(dirPath.entryInfoList()[0].absoluteFilePath());
        }
        // 7. imu file
        dirPath.setNameFilters(QStringList(QString("*-IMU.csv")));
        if (dirPath.entryInfoList().size() <= 0) {
            qDebug() << "Can't find *-IMU.csv\n";
            showMessageBox(QString("Cannot open *-IMU.csv!"));
        }
        else {
            this->imuFile.push_back(dirPath.entryInfoList()[0].absoluteFilePath());\
        }
        // 8. 目标探测比赛真值（如果没有，则认为当前比赛科目是场景理解
        dirPath.setNameFilters(QStringList(QString("*-Groundtruth")));
        QDir gtPath(dirPath.entryInfoList()[0].absoluteFilePath());
        gtPath.setNameFilters(QStringList(QString("gt_detection.csv")));
        if (gtPath.entryInfoList().size() > 0) {
            this->competitionType.push_back(OBJECT_DETECTION);
            this->gtDetection.push_back(gtPath.entryInfoList()[0].absoluteFilePath());

            this->gtGlobalPng.push_back(QString("NULL"));
            this->gtLocalization.push_back(QString("NULL"));
            this->gtGlobal.push_back(QString("NULL"));
            this->gtSceneUnderstanding.push_back(QString("NULL"));
        }
        else {
            this->competitionType.push_back(SCENE_UNDERSTANDING);
            dirPath.setNameFilters(QStringList(QString("*-Groundtruth")));
            QDir gtPath(dirPath.entryInfoList()[0].absoluteFilePath()+"/files/");
            // ------------当前科目是场景理解------------
            // 读入场景理解的任务地图文件
            gtPath.setNameFilters(QStringList(QString("global.png")));
            assert(gtPath.entryInfoList().size() > 0);
            this->gtGlobalPng.push_back(gtPath.entryInfoList()[0].absoluteFilePath());
            // 读入定位考核点真值
            gtPath = dirPath.entryInfoList()[0].absoluteFilePath();
            gtPath.setNameFilters(QStringList(QString("gt_local.csv")));
            assert(gtPath.entryInfoList().size() > 0);
            this->gtLocalization.push_back(gtPath.entryInfoList()[0].absoluteFilePath());
            // 读入场景理解考核点
            gtPath.setNameFilters(QStringList(QString("gt_mp.csv")));
            assert(gtPath.entryInfoList().size() > 0);
            this->gtSceneUnderstanding.push_back(gtPath.entryInfoList()[0].absoluteFilePath());
            // 读入建图考核点
            gtPath.setNameFilters(QStringList(QString("gt_global.csv")));
            assert(gtPath.entryInfoList().size() > 0);
            this->gtGlobal.push_back(gtPath.entryInfoList()[0].absoluteFilePath());

            this->gtDetection.push_back(QString("NULL"));
        }
    }

    // Read player information file
    FILE *fPlayer = fopen(playerInfoFile.c_str(), "r");
    if (fPlayer == NULL) {
        qDebug() << "Can't open player info file\n";
        showMessageBox(QString("Cannot open player info file!"));
    }
    else {
        char tag[50];
        char ss[50];
        int playerId = 0, ret;
        while (fscanf(fPlayer, "%[^,],%*[ ]%s\n", &tag, &ss) != EOF) {
             CPlayer newPlayer(-1, "unknown", "0.0.0.0");
            newPlayer.id = playerId ++;
            if (strcmp(tag, "name") != 0) {
                qDebug() << "Player name information error!";
                getchar();
                exit(-1);
            }
            newPlayer.name = std::string(ss);
            ret = fscanf(fPlayer, "%[^,],%*[ ]%s\n", &tag, &ss);
            if (strcmp(tag, "ip") != 0 || ret==0) {
                qDebug() << "Player IP information error!";
                getchar();
                exit(-1);
            }
            newPlayer.ip = std::string(ss);
            // read empty line
            fscanf(fPlayer, "%*[\n]");
            this->playerVec.push_back(newPlayer);
        }
    }
}

void CConfig::showMessageBox(const QString &text) {
  QMessageBox msgBox;
  msgBox.setText(text);
  msgBox.exec();
//    close();
}

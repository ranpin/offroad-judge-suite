#ifndef CSERVER_H
#define CSERVER_H

// 准备考试：0x023003
// 考试结束：0x023303
// 准备完成：0x023203
// 每条令牌报文共12字节，其结构为  0x02开始  +  时间戳：8字节(long long)  +  令牌类型：1字节(uchar)  +  令牌值：1字节(uchar)  +  0x03结束
// 令牌类型为1个大写英文字母，占1字节(uchar)，令牌值是1~9之间的数，占1字节(uchar)
// 令牌类型对应的字符如下:
// 字符    ASCII码      令牌类型
//  C       0x43        单目相机
//  G       0x47        CANBUS 档位
//  O       0x4F        CANBUS 编码器
//  S       0x53        CANBUS 方向盘
//  P       0x50        GPS
//  U       0x55        惯导
//  L       0x4C        多线激光雷达
//  R       0x52        单线激光雷达

#include <QtNetwork/QTcpServer>
#include <QThreadPool>
#include <QDebug>
#include "header.h"
#include "main_window.hpp"
#include "cconfig.h"
#include "ctask.h"
#include "imagePublisher.hpp"
#include "gpsPublisher.hpp"
#include "suGtPublisher.hpp"
#include "odGtPublisher.h"
#include "tfPublisher.hpp"
#include "selectPointSubscriber.h"
#include "ctimermessagebox.h"

class CServer : public QTcpServer
{
    Q_OBJECT
public:
    // Initialize threadpool, get config info, read data tokens from file.
    explicit CServer(QObject *parent = 0);
    explicit CServer(int argc, char** argv, Ui::MainWindowDesign *ui, QObject *parent = 0);
    ~CServer();

    // The main function of the server:
    // scan player's IP and send "Prepare competition".
    void startServer();
    void showMessageBox(const QString &text);
    Calib loadVeloCalib(QString filePath);
    QString getQuaternionFromCalib(Calib &_calib);
    void startQProcess(QProcess *&processPtr, QString cmdStr);
    void killQProcess(QProcess *&processPtr);
    void killDataPlayer();

signals:

public slots:
    void slotNewConnection();
    void slotReadyReadCommand();    // read command "prepared"
    void slotButtonPrepareCompetition();    // send command "prepare competition"
    void slotButtonSendToken();     // send data tokens
    void slotButtonEndCompetition();      // send command "end competition"
    void slotAppendText(const QString &text);
    void slotChangeLabel(const int id, const QString &text);
    void slotChangeProgress(const int idx, const int progress);
    void slotChangeCompetitionReadyState(int state);

private:
    QTcpSocket *mainSocket;
public:
    int init_argc;
    char** init_argv;
    CConfig config;
    std::vector<QCheckBox*> playerState;
    std::vector<QLabel*> playerQLabel;
    std::vector<QProgressBar*> playerDataProgress;
    std::vector<CDataToken> dataTokens;
    std::vector<long long> timeStampList;       // 时间戳列表
    std::vector<QString> imgNameList;       // 图像文件名列表
    std::map<std::string, CPlayer> playerInfo;
    std::vector<CTask*> taskVec;
    QProcess *veloPublisher;
    QProcess *carModelPublisher;
    Calib veloCalib;
    server_node::imagePublisher *imgPub;
    server_node::gpsPublisher *gpsPub;
    server_node::suGtPublisher *suGtPub;
    server_node::odGtPublisher *odGtPub;
    server_node::tfPublisher *tfPub;
    server_node::selectPointSubscriber *selectPointSub;
    Ui::MainWindowDesign *ui;
    bool hasKillDataPlayer;
};

#endif // CSERVER_H

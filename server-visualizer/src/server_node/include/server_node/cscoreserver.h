#ifndef CSCORESERVER_H
#define CSCORESERVER_H

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
#include "cconfig.h"
#include "main_window.hpp"

class CScoreServer : public QTcpServer
{
    Q_OBJECT
public:
    // Initialize threadpool, get config info, read data tokens from file.
    explicit CScoreServer(QObject *parent = 0);
    explicit CScoreServer(Ui::MainWindowDesign *ui, QObject *parent = 0);

    // The main function of the server:
    // listen score port;
    void startServer();
    // 更新排名表格
    void updateScoreRank();

signals:

public slots:
    void slotNewConnection();
    void slotReadyReadCommand();    // read command "prepared"
    void slotAppendText(const QString &text);

private:
    QTcpSocket *mainSocket;
public:
    CConfig config;
    std::map<std::string, CPlayer> playerInfo;
    std::map<std::string, int> playerScore;
    Ui::MainWindowDesign *ui;
    QFile scoreFile;
    QString fileName; //文件名字
    qint64 fileSize; //文件大小
    qint64 recvSize; //接收长度
    bool isFileHead; //判断是否是文件头
};

#endif // CSCORESERVER_H

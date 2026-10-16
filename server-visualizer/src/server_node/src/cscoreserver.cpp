#include "../include/server_node/cscoreserver.h"

CScoreServer::CScoreServer(Ui::MainWindowDesign *ui, QObject *parent) : QTcpServer(parent)
{
    this->ui = ui;
    // TCP connect
    connect(this, SIGNAL(newConnection()), this, SLOT(slotNewConnection()));

    // get players' information
    for (auto p : config.playerVec) {
        this->playerInfo[p.ip] = CPlayer(p);
    }

    // init scores table
    this->ui->tableWidget->setColumnCount(2);
    this->ui->tableWidget->setRowCount(int(this->config.playerVec.size()));
    QStringList tabHeader;
    tabHeader << "队名" << "总分";
    this->ui->tableWidget->setHorizontalHeaderLabels(tabHeader);
    this->ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int i = 0; i < config.playerVec.size(); ++i) {
        this->playerScore[config.playerVec[i].ip] = 0;  // 初始得分为0
    }
    // 更新得分排名表
    updateScoreRank();
}

void CScoreServer::startServer()
{
    // The main function of the server:
    // listen score of players
    if (this->listen(QHostAddress::Any, config.scoreListenPort))
    {
        qDebug() << "Server started.";
        slotAppendText(QString("服务器已启动,监听得分端口%1").arg(config.scoreListenPort));
    }
    else
    {
        qDebug() << "Server did not start!";
        slotAppendText(QString("服务器启动失败![监听端口%1失败]").arg(config.scoreListenPort));
    }
    this->isFileHead = true;
}

void CScoreServer::updateScoreRank()
{
    for (int i = 0; i < config.playerVec.size(); ++i) {
        this->ui->tableWidget->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(this->config.playerVec[i].name)));
        this->ui->tableWidget->item(i,0)->setTextAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
        this->ui->tableWidget->setItem(i, 1, new QTableWidgetItem(QString("%1").arg(this->playerScore[config.playerVec[i].ip])));
        this->ui->tableWidget->item(i,1)->setTextAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
    }
    this->ui->tableWidget->sortItems(1, Qt::DescendingOrder);
}

// Listen score files of players
void CScoreServer::slotReadyReadCommand()
{
    QByteArray data = this->mainSocket->readAll();
    // 得分数据的格式: 0x02 + int +0x03
    // 判断格式是否完整
    bool legalFlag = false;
    std::string playerIp = QHostAddress(this->mainSocket->peerAddress().toIPv4Address()).toString().toStdString();
    if (data.at(0)==0x02 && data.at(data.size()-1)==0x03 && data.size()==6) {
        int curScore;
        std::memcpy(&curScore, data.mid(1,4), sizeof(int));
        if (curScore >= 0) {
            legalFlag = true;  // 成功解析得分
            this->playerScore[playerIp] = curScore;
            slotAppendText(QString("<font color=\"#00B400\">[得分] 收到得分%4, 来自用户%1(%2 at %3) </font>").arg(this->playerInfo[playerIp].id)
                                                                            .arg(QString::fromStdString(this->playerInfo[playerIp].name))
                                                                            .arg(QString::fromStdString(this->playerInfo[playerIp].ip))
                                                                            .arg(curScore));
            // 在排行榜中更新得分
            updateScoreRank();
        }
    }
    if (!legalFlag) {  // 得分信息解析失败
        slotAppendText(QString("<font color=\"#FF0000\">[失败] 无法解析得分信号%4, 来自用户%1(%2 at %3) </font>").arg(this->playerInfo[playerIp].id)
                                                                        .arg(QString::fromStdString(this->playerInfo[playerIp].name))
                                                                        .arg(QString::fromStdString(this->playerInfo[playerIp].ip))
                                                                        .arg(QString(data.toHex())));
    }
    //接收得分文件
//   QByteArray buf = this->mainSocket->readAll();

//   if (this->isFileHead)//判断是否是文件头
//   {
//       this->isFileHead = false;
//       //解析头部 "文件名#文件大小#总分"
//       fileName = QString(buf).section("#", 0, 0);
//       fileSize = QString(buf).section("#", 1, 1).toInt();
//       int totScore = QString(buf).section("#", 2, 2).toInt();
//       recvSize = 0;

//       this->scoreFile.setFileName(fileName);

//      //打开文件
//       bool isOK = this->scoreFile.open(QIODevice::WriteOnly);
//       if (!isOK) {
//           qDebug() << "得分文件创建失败";
//           slotAppendText(QString("<font color=\"#FF0000\">[错误] 得分文件创建失败, 来自用户%1(%2 at %3) </font>").arg(this->playerInfo[playerIp].id)
//                                                                           .arg(QString::fromStdString(this->playerInfo[playerIp].name))
//                                                                           .arg(QString::fromStdString(this->playerInfo[playerIp].ip)));
//       }
//   }
//   else {   // 读文件内容的数据流
//           qDebug() << "开始保存得分文件";
//           qint64 len = this->scoreFile.write(buf);
//           recvSize += len;

//           qDebug()  << "接收得分文件..." <<recvSize;
//           if(recvSize == fileSize)
//           {
//               qDebug() << "完成: 接受得分文件成功!";
//               slotAppendText(QString("<font color=\"#00B400\">[得分] 得分文件接收成功, 来自用户%1(%2 at %3) </font>").arg(this->playerInfo[playerIp].id)
//                                                                               .arg(QString::fromStdString(this->playerInfo[playerIp].name))
//                                                                               .arg(QString::fromStdString(this->playerInfo[playerIp].ip)));
//               this->scoreFile.close();
//               this->mainSocket->disconnectFromHost();
//               this->mainSocket->close();
//           }
//       }
}

void CScoreServer::slotNewConnection()
{
    // Process scores of players
    this->mainSocket = this->nextPendingConnection();
    qDebug() << "(scoreServer) TCP connection with " << QHostAddress(this->mainSocket->peerAddress().toIPv4Address()).toString();
    slotAppendText(QString("<font color=\"#00B400\">ScoreServer TCP connection from %1 </font>").arg(QHostAddress(this->mainSocket->peerAddress().toIPv4Address()).toString()));

    // Listen scores of players
    connect(this->mainSocket, SIGNAL(readyRead()), this, SLOT(slotReadyReadCommand()));
}

void CScoreServer::slotAppendText(const QString &text)
{
    // append text to log window
    QString curTime = QDateTime::currentDateTime().toString("[yyyy.MM.dd hh:mm:ss.zzz] ");
    this->ui->textBrowser->append(curTime + text);
}

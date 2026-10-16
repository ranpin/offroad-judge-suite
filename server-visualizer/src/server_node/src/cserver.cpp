#include "../include/server_node/cserver.h"
#include "../include/server_node/ctask.h"

CServer::CServer(int argc, char** argv, Ui::MainWindowDesign *ui, QObject *parent) : QTcpServer(parent),
  init_argc(argc),
  init_argv(argv),
  ui(ui)
{
    // 初始化一些指针
    this->suGtPub = nullptr;
    this->odGtPub = nullptr;
    this->imgPub= nullptr;
    this->gpsPub= nullptr;
    // UI connect
    connect(this->ui->pushButtonPrepareCompetition, SIGNAL(clicked()), this, SLOT(slotButtonPrepareCompetition()));
    connect(this->ui->pushButtonStartCompetition, SIGNAL(clicked()), this, SLOT(slotButtonSendToken()));
    connect(this->ui->pushButtonEndCompetition, SIGNAL(clicked()), this, SLOT(slotButtonEndCompetition()));
    // TCP connect
    connect(this, SIGNAL(newConnection()), this, SLOT(slotNewConnection()));

    // get players' information
    this->ui->scrollAreaWidgetContents->setMaximumWidth(300);
    this->ui->scrollArea->setMaximumWidth(300);
    for (int i = 0; i < config.playerVec.size(); ++i) {
        this->playerInfo[config.playerVec[i].ip] = config.playerVec[i];
        // 增加每个车队的准备完成状态 和 数据传输进度条
        playerState.push_back(new QCheckBox());
        playerState[i]->setChecked(false);
        playerState[i]->setText(QString::fromStdString(config.playerVec[i].name + " @ " + config.playerVec[i].ip));
        connect(playerState[i], SIGNAL(stateChanged(int)), this, SLOT(slotChangeCompetitionReadyState(int)));
        this->ui->verticalLayout_scrollArea->addWidget(playerState[i]);
        playerQLabel.push_back(new QLabel(QString("考试进度")));
        this->ui->verticalLayout_scrollArea->addWidget(playerQLabel[i]);
        playerDataProgress.push_back(new QProgressBar());
        playerDataProgress[i]->setRange(0,100);
        playerDataProgress[i]->setValue(0);
        playerDataProgress[i]->setMaximumWidth(300);
        this->ui->verticalLayout_scrollArea->addWidget(playerDataProgress[i]);
    }
    this->ui->verticalLayout_scrollArea->addStretch();

    // 手动选点的监听node
    this->selectPointSub = new server_node::selectPointSubscriber(init_argc, init_argv, &config);
    this->selectPointSub->init();
    connect(this->selectPointSub, SIGNAL(signalAppendText(QString)), this, SLOT(slotAppendText(QString)));
}

CServer::~CServer() {
    // 停止播放数据,kill对应的rosnode
    killQProcess(this->veloPublisher);
    killQProcess(this->carModelPublisher);
    // 停止播放相机数据
    if (this->imgPub != nullptr) delete this->imgPub;
    // 停止播放GPS/IMU数据
    if (this->gpsPub != nullptr) delete this->gpsPub;
    if (this->tfPub != nullptr) delete this->tfPub;
    if (this->odGtPub != nullptr) delete this->odGtPub;
    if (this->suGtPub != nullptr) delete this->suGtPub;
    if (this->selectPointSub != nullptr) delete this->selectPointSub;
}

void CServer::startServer()
{
    // The main function of the server:
    // listen requestListenPort port (listen "Prepared");
    if (this->listen(QHostAddress::Any, config.requestListenPort))
    {
        qDebug() << "Server started.";
        slotAppendText(QString("服务器已启动,监听考试指令端口%1").arg(config.requestListenPort));
    }
    else
    {
        qDebug() << "Server did not start!";
        slotAppendText(QString("服务器启动失败![监听端口%1失败]").arg(config.requestListenPort));
    }
}

Calib CServer::loadVeloCalib(QString filePath)
{
    Calib ret;
    FILE *f = std::fopen(filePath.toStdString().c_str(), "r");
    fscanf(f, "rot %lf %lf %lf\n", &ret.rot[0], &ret.rot[1], &ret.rot[2]);
    fscanf(f, "shv %lf %lf %lf\n", &ret.shv[0], &ret.shv[1], &ret.shv[2]);
    std::fclose(f);
    return ret;
}

void CServer::startQProcess(QProcess *&processPtr, QString cmdStr)
{
    // 调用QProcess执行系统命令（cmdStr), 例如roslaunch
    processPtr = new QProcess();
    processPtr->setProcessChannelMode(QProcess::MergedChannels);
    processPtr->start(cmdStr);
    qDebug() << cmdStr;
    slotAppendText(QString("<font color=\"#0000FF\"> %1 </font>").arg(cmdStr));
//    connect(processPtr, &QProcess::readyReadStandardOutput, [&]()
//    {
//        QString outputStr = processPtr->readAllStandardOutput();
//        qDebug() << outputStr;
//        slotAppendText(outputStr);
//    });
    connect(processPtr, SIGNAL(finished(int)), processPtr, SLOT(deleteLater()));
    processPtr->waitForStarted();
}

void CServer::killQProcess(QProcess *&processPtr)
{
    QProcess* kill_process = new QProcess;
    QString str = "kill -SIGINT " + QString::number(processPtr->processId());
    qDebug() << str;
    kill_process->start(str);
//    connect(kill_process, &QProcess::readyReadStandardOutput, [&]()
//    {
//        QString outputStr = kill_process->readAllStandardOutput();
//        qDebug() << outputStr;
//        slotAppendText(outputStr);
//    });
    connect(kill_process, SIGNAL(finished(int)), kill_process, SLOT(deleteLater()));
}

QString CServer::getQuaternionFromCalib(Calib &_calib)
{
    // ROS坐标系是前x左y，PKU坐标系是右x前y
    tf::Quaternion q;
    q.setRPY(-_calib.rot[1]*M_PI/180.0, _calib.rot[0]*M_PI/180.0, -_calib.rot[2]*M_PI/180.0);
    // shv: x, y, z, rot: x,y,z,w
    return QString("%1 %2 %3 %4 %5 %6 %7").arg(_calib.shv[1]).arg(-_calib.shv[0]).arg(-_calib.shv[2])
                                          .arg(q.x()).arg(q.y()).arg(q.z()).arg(q.w());
}

// Send "Prepare competition" command
void CServer::slotButtonPrepareCompetition()
{
    // scan player's IP and send "Prepare competition"
    for (auto curPlayer : config.playerVec) {
         CTask *task = new CTask(curPlayer, this->config, this->dataTokens, TASK_PREPARE_COMPETITION, this->ui, this);
         // textBrowser.append need to be invoked in main thread, so make an interface to it.
         connect(task, SIGNAL(signalAppendText(QString)), this, SLOT(slotAppendText(QString)));
         qDebug() << QString("Thread start for player %1 : ").arg(curPlayer.id) + QString::fromStdString(curPlayer.ip + ',' + curPlayer.name);
         task->start();
    }
}

// Send "End competition" command
void CServer::slotButtonEndCompetition()
{
    //停止发送数据令牌
    for (auto &taski : this->taskVec) {
        taski->needStop = true;
    }
    this->taskVec.clear();
    this->ui->pushButtonStartCompetition->setText("开始考试 (发送数据令牌)");
    this->ui->pushButtonStartCompetition->setChecked(false);
    // scan player's IP and send "End competition"
    for (auto curPlayer : config.playerVec) {
         CTask *task = new CTask(curPlayer, this->config, this->dataTokens, TASK_END_COMPETITION, this->ui, this);
         // textBrowser.append need to be invoked in main thread, so make an interface to it.
         connect(task, SIGNAL(signalAppendText(QString)), this, SLOT(slotAppendText(QString)));
         qDebug() << QString("Thread start for player %1 : ").arg(curPlayer.id) + QString::fromStdString(curPlayer.ip + ',' + curPlayer.name);
         task->start();
    }
    killDataPlayer();
}

void CServer::killDataPlayer()
{
    if (hasKillDataPlayer) return;
    // 停止播放数据,kill对应的rosnode
    killQProcess(this->veloPublisher);
    killQProcess(this->carModelPublisher);
    // 停止播放相机数据
    if (this->imgPub != nullptr) {delete this->imgPub; this->imgPub = nullptr;}
    // 停止播放GPS/IMU数据
    if (this->gpsPub != nullptr) {delete this->gpsPub; this->gpsPub = nullptr;}
    if (this->tfPub != nullptr) {delete this->tfPub; this->tfPub = nullptr;}
    if (this->odGtPub != nullptr) {delete this->odGtPub; this->odGtPub = nullptr;}
    if (this->suGtPub != nullptr) {delete this->suGtPub; this->suGtPub = nullptr;}
    if (this->selectPointSub != nullptr) {delete this->selectPointSub; this->selectPointSub = nullptr;}

    // 前面的delete操作会终止ros node handler, 要手动启动选点的监听node，保证其正常运行
    this->selectPointSub = new server_node::selectPointSubscriber(init_argc, init_argv, &config);
    this->selectPointSub->init();
    connect(this->selectPointSub, SIGNAL(signalAppendText(QString)), this, SLOT(slotAppendText(QString)));

    hasKillDataPlayer = true;

    // 终止数据播放后（可能是手动停止或者播放结束），自动播放下一段数据
    if (this->config.curDataset < this->config.dataPath.length()-1) {
        server_node::cTimerMessageBox nextCompetitionBox;
        nextCompetitionBox.start();
        ++ this->config.curDataset;
        if (nextCompetitionBox.playNext) {
            // 通过按钮slot函数直接开始发送下一段考试令牌
            this->ui->pushButtonStartCompetition->setChecked(true);
            slotButtonSendToken();
        }
    }
}

// send data tokens to each player
void CServer::slotButtonSendToken()
{
    if (this->ui->pushButtonStartCompetition->isChecked()) {
        hasKillDataPlayer = false;
        // 改变按钮状态
        this->ui->pushButtonStartCompetition->setText("停止发送数据令牌");
        // 重置数据发送进度条
        for (int i = 0; i < config.playerVec.size(); ++i) {
            playerDataProgress[i]->setValue(0);
            playerDataProgress[i]->setStyleSheet("QProgressBar::chunk {background-color: #05B8CC;} "
                                                "QProgressBar{text-align: center;}");
        }
        slotAppendText(QString("<font color=\"#FF00FF\">开始播放考题%1:%2</font>").arg(this->config.curDataset+1).arg(this->config.dataPath[this->config.curDataset]));
        // -------------------读数据--------------------
        // 1. Read time stamp file
        FILE *fts = fopen(config.timeStampFile[config.curDataset].toStdString().c_str(), "r");
        fscanf(fts, "%*[^\n]");
        long long ts;
        char imgName[100];
        while (fscanf(fts, "%lld,%s\n", &ts,  &imgName) != EOF) {
            if (ts <= 0) {
                qDebug() << "Time stamp read error!";
                showMessageBox(QString("Time stamp read error!"));
                exit(-1);
            }
            this->timeStampList.push_back(ts);
            this->imgNameList.push_back(QDir::cleanPath(config.cameraFolder[config.curDataset]+QDir::separator()+QString(imgName)));
        }
        std::fclose(fts);

        // 2. 读取data token
        FILE *ftoken = fopen(config.tokenFile[config.curDataset].toStdString().c_str(), "r");
        fscanf(ftoken, "%*[^\n]");  // 第一行标题，读入并忽略
        uchar _type;
        unsigned int _token;  // 实际数值范围uchar
        while (fscanf(ftoken, "%lld,%c,%d\n", &ts, &_type, &_token) != EOF) {
          this->dataTokens.push_back(CDataToken(ts, _type, (uchar)(_token & 0xFF)));
        }
        std::fclose(ftoken);
        // 3. 读取velodyne标定参数（到车体的参数），用于可视化rviz car model
        this->veloCalib = loadVeloCalib(config.veloCalib[config.curDataset]);

        // -------------------播放数据--------------------
        // 1. send "start competition" command
        for (auto curPlayer : config.playerVec) {
             CTask *task = new CTask(curPlayer, this->config, this->dataTokens, TASK_START_COMPETITION, this->ui, this);
             connect(task, SIGNAL(signalAppendText(QString)), this, SLOT(slotAppendText(QString)));
             qDebug() << QString("Thread start for player %1 : ").arg(curPlayer.id) + QString::fromStdString(curPlayer.ip + ',' + curPlayer.name);
             task->start();
        }
        QString cmdStr;
        // 2. roslaunch car_model
        cmdStr = QString("roslaunch rviz_car_model default.launch fixed_frame:=\"velodyne\" transform:=\"%1\"")
                    .arg(getQuaternionFromCalib(this->veloCalib));
        startQProcess(carModelPublisher, cmdStr);
        // 3. play GPS/IMU data
        this->gpsPub = new server_node::gpsPublisher(init_argc, init_argv, &veloCalib, &config);
        this->gpsPub->init();
        // 4. publish Ground Truth
        if (config.competitionType[config.curDataset] == SCENE_UNDERSTANDING) {
            // 场景理解考试科目
            this->suGtPub = new server_node::suGtPublisher(init_argc, init_argv, &config);
            this->suGtPub->init();
        }
        else if (config.competitionType[config.curDataset] == OBJECT_DETECTION) {
            this->odGtPub = new server_node::odGtPublisher(init_argc, init_argv, &config);
            this->odGtPub->init();
        }
        // 5.启动tf publisher
        this->tfPub = new server_node::tfPublisher(init_argc, init_argv, &veloCalib, &config);
        this->tfPub->init();

        // 6. play camera data
        this->imgPub = new server_node::imagePublisher(init_argc, init_argv, this->timeStampList, this->imgNameList, this->odGtPub);
        this->imgPub->init();
        // 7. play Velodyne data (pcap)
        cmdStr = QString("roslaunch velodyne_pointcloud 32e_points.launch pcap:=%1").arg(config.veloFile[config.curDataset]);
        startQProcess(veloPublisher, cmdStr);

        // scan player's IP and send data token
        for (int i = 0; i < config.playerVec.size(); i ++) {
             CPlayer curPlayer = config.playerVec[i];
             this->taskVec.push_back(new CTask(curPlayer, this->config, this->dataTokens, TASK_SEND_DATA_TOKEN, this->ui, this));
             // textBrowser.append need to be invoked in main thread, so make an interface to it.
             connect(this->taskVec[i], SIGNAL(signalAppendText(QString)), this, SLOT(slotAppendText(QString)));
             connect(this->taskVec[i], SIGNAL(signalChangeProgress(int,int)), this, SLOT(slotChangeProgress(int,int)));
             connect(this->taskVec[i], SIGNAL(signalChangeLabel(int, QString)), this, SLOT(slotChangeLabel(int,QString)));
             qDebug() << QString("Data token thread start for player %1 : ").arg(curPlayer.id) + QString::fromStdString(curPlayer.ip + ',' + curPlayer.name);
             this->taskVec[i]->start();
        }
    }
    else {  //手动停止发送数据令牌
        for (auto &taski : this->taskVec) {
            taski->needStop = true;
        }
        this->taskVec.clear();
        this->ui->pushButtonStartCompetition->setText("开始考试 (发送数据令牌)");
        this->ui->pushButtonStartCompetition->setChecked(false);
        // 如果是环境建模科目，显示global.png图像
        if (config.competitionType[config.curDataset] == SCENE_UNDERSTANDING && this->config.gtGlobalPng[this->config.curDataset] != QString("NULL")) {
            cv::Mat globalPng = cv::imread(this->config.gtGlobalPng[this->config.curDataset].toStdString());
            cv::imshow("global", globalPng);
            cv::waitKey(0);
        }
        // 停止播放数据,kill对应的rosnode
        killDataPlayer();
    }
}

// Listen "Prepared" command
void CServer::slotReadyReadCommand()
{
    QByteArray data = this->mainSocket->readAll();
    // check whether the data is "Prepared" command
    const QByteArray cmd_CompetitionReady = QByteArray::fromRawData(cmdCompetitionReady, sizeof(cmdCompetitionReady));
    const QByteArray cmd_AnswerOver = QByteArray::fromRawData(cmdAnswerOver, sizeof(cmdAnswerOver));

    // find which player it is
    std::string playerIp = QHostAddress(this->mainSocket->peerAddress().toIPv4Address()).toString().toStdString();
    // check if a illegal IP
    if (this->playerInfo.find(playerIp) == this->playerInfo.end()) {
        qDebug() << "Unkown IP: " << QString::fromStdString(playerIp);
        slotAppendText(QString("<font color=\"#FF0000\">[未知IP]来自 %1</font>").arg(QString::fromStdString(playerIp)));
        return;
    }
    if (data == cmd_CompetitionReady) {
        if (this->playerState[this->playerInfo[playerIp].id]->isChecked()) {
            // 如果已经checked,手动打印信息；否则，以下由QCheckBox的信号触发
            qDebug() << QString("Player %1 (%2 at %3) is ready.").arg(this->playerInfo[playerIp].id)
                                                                .arg(QString::fromStdString(this->playerInfo[playerIp].name))
                                                                .arg(QString::fromStdString(this->playerInfo[playerIp].ip));
            slotAppendText(QString("<font color=\"#00B400\">用户%1 (%2 at %3)准备完成 </font>").arg(this->playerInfo[playerIp].id)
                                                                            .arg(QString::fromStdString(this->playerInfo[playerIp].name))
                                                                            .arg(QString::fromStdString(this->playerInfo[playerIp].ip)));
        }
        else {
            // player is ready for competition
            this->playerState[this->playerInfo[playerIp].id]->setChecked(true);
        }
    }
    else  if (data == cmd_AnswerOver) {
        qDebug() << QString("Player %1 (%2 at %3) answer completed.").arg(this->playerInfo[playerIp].id)
                                                            .arg(QString::fromStdString(this->playerInfo[playerIp].name))
                                                            .arg(QString::fromStdString(this->playerInfo[playerIp].ip));
        slotAppendText(QString("<font color=\"#00B400\">用户%1 (%2 at %3)作答完成 </font>").arg(this->playerInfo[playerIp].id)
                                                                        .arg(QString::fromStdString(this->playerInfo[playerIp].name))
                                                                        .arg(QString::fromStdString(this->playerInfo[playerIp].ip)));
    }
    else {
        if (!this->playerState[this->playerInfo[playerIp].id]->isChecked())
            this->playerState[this->playerInfo[playerIp].id]->setChecked(false);
        qDebug() << QString("[Command type error] Receive %4 from Player %1(%2 at %3)").arg(this->playerInfo[playerIp].id)
                                                                                        .arg(QString::fromStdString(this->playerInfo[playerIp].name))
                                                                                        .arg(QString::fromStdString(this->playerInfo[playerIp].ip))
                                                                                        .arg(QString(data.toHex()));
        slotAppendText(QString("<font color=\"#FF0000\">[未知指令] 收到指令 [%4], 来自用户%1(%2 at %3)</font>").arg(this->playerInfo[playerIp].id)
                                                                        .arg(QString::fromStdString(this->playerInfo[playerIp].name))
                                                                        .arg(QString::fromStdString(this->playerInfo[playerIp].ip))
                                                                        .arg(QString(data.toHex())));
        this->ui->pushButtonStartCompetition->setChecked(false);
    }
}

void CServer::showMessageBox(const QString &text) {
  QMessageBox msgBox;
  msgBox.setText(text);
  msgBox.exec();
//    close();
}

void CServer::slotNewConnection()
{
    // listen "prepared" command.
    this->mainSocket = this->nextPendingConnection();
    qDebug() << "(server) TCP connection with " << QHostAddress(this->mainSocket->peerAddress().toIPv4Address()).toString();
    slotAppendText(QString("<font color=\"#00B400\">TCP connection from %1 </font>").arg(QHostAddress(this->mainSocket->peerAddress().toIPv4Address()).toString()));

    // Listen "Prepared" command
    connect(this->mainSocket, SIGNAL(readyRead()), this, SLOT(slotReadyReadCommand()));
}

void CServer::slotAppendText(const QString &text)
{
    // append text to log window
    QString curTime = QDateTime::currentDateTime().toString("[MM.dd hh:mm:ss.zzz] ");
    this->ui->textBrowser->append(curTime + text);
}

void CServer::slotChangeLabel(const int id, const QString &text)
{
    this->playerQLabel[id]->setText(text);
}

void CServer::slotChangeProgress(const int idx, const int progress)
{
    // append text to log window
    if (progress >= 0)
        this->playerDataProgress[idx]->setValue(progress);
    else {
        this->playerDataProgress[idx]->setStyleSheet("QProgressBar::chunk {background-color: red;} "
                                                     "QProgressBar{text-align: center;}");
    }
    // 完成数据发送后，停止播放数据
    if (progress == 100) {
//        for (auto &taski : this->taskVec) {
//            taski->needStop = true;
//        }
        this->taskVec[idx]->needStop = true;
    }
    // 如果所有车队都发完了,就关闭数据播放进程
    bool allPlayerFinish = true;
    for (int i = 0; i < this->playerDataProgress.size(); ++i) {
        if (this->playerDataProgress[i]->value() != 100)
            allPlayerFinish = false;
    }
    if (allPlayerFinish) {
        this->taskVec.clear();
        this->ui->pushButtonStartCompetition->setText("开始考试 (发送数据令牌)");
        this->ui->pushButtonStartCompetition->setChecked(false);
        // 停止播放数据,kill对应的rosnode
        killDataPlayer();
        // 如果是环境建模科目，显示global.png图像
        if (config.competitionType[config.curDataset] == SCENE_UNDERSTANDING && this->config.gtGlobalPng[this->config.curDataset] != QString("NULL")) {
            cv::Mat globalPng = cv::imread(this->config.gtGlobalPng[this->config.curDataset].toStdString());
            cv::imshow("global", globalPng);
            cv::waitKey(0);
        }
    }
}

void CServer::slotChangeCompetitionReadyState(int state)
{
    QCheckBox *curSender = qobject_cast<QCheckBox*>(sender());
    // 车队准备完成 或 手动设置准备完成的状态
    if (state == Qt::Checked) {
        curSender->setChecked(true);
        // 找一下是哪个车队状态改变了
        for (int i = 0; i < playerState.size(); ++i) {
            if (playerState[i] == curSender) {
                std::string playerIp = config.playerVec[i].ip;
                curSender->setText(QString::fromStdString(playerInfo[playerIp].name + " @ " + config.playerVec[i].ip+"[准备完成]"));
                qDebug() << QString("Player %1 (%2 at %3) is ready.").arg(this->playerInfo[playerIp].id)
                                                                    .arg(QString::fromStdString(this->playerInfo[playerIp].name))
                                                                    .arg(QString::fromStdString(this->playerInfo[playerIp].ip));
                slotAppendText(QString("<font color=\"#00B400\">用户%1 (%2 at %3)准备完成 </font>").arg(this->playerInfo[playerIp].id)
                                                                                .arg(QString::fromStdString(this->playerInfo[playerIp].name))
                                                                                .arg(QString::fromStdString(this->playerInfo[playerIp].ip)));
            }
        }
    }
    // 准备完成状态被手动取消
    else if (state == Qt::Unchecked) {
        curSender->setChecked(false);
        // 找一下是哪个车队状态改变了
        for (int i = 0; i < playerState.size(); ++i) {
            if (playerState[i] == curSender) {
                std::string playerIp = config.playerVec[i].ip;
                curSender->setText(QString::fromStdString(playerInfo[playerIp].name + " @ " + config.playerVec[i].ip+"[未准备]"));
                qDebug() << QString("[Warning] Player %1(%2 at %3) is not prepared!").arg(this->playerInfo[playerIp].id)
                                                                                                .arg(QString::fromStdString(this->playerInfo[playerIp].name))
                                                                                                .arg(QString::fromStdString(this->playerInfo[playerIp].ip));
                slotAppendText(QString("<font color=\"#FF0000\">[警告] 用户%1(%2 at %3)未准备</font>").arg(this->playerInfo[playerIp].id)
                                                                                .arg(QString::fromStdString(this->playerInfo[playerIp].name))
                                                                                .arg(QString::fromStdString(this->playerInfo[playerIp].ip)));
            }
        }
    }
}

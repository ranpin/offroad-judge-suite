#include "../include/server_node/ctask.h"

CTask::CTask()
{
    qDebug() << "create a CTask without construction parameters.";
}

CTask::CTask(CPlayer player, CConfig &config, std::vector<CDataToken> &dataTokens, int taskID,
             Ui::MainWindowDesign *ui, QObject *parent): QThread(parent),
    player(player), config(&config), dataTokens(&dataTokens), taskID(taskID), ui(ui), timer(nullptr)
{
    // taskID = 0 : send 'Prepare Competition'
    // taskID = 1 : send data tokens
    if (this->taskID == TASK_PREPARE_COMPETITION || this->taskID == TASK_END_COMPETITION || this->taskID == TASK_START_COMPETITION)
        port = config.clientCommandPort;
    if (this->taskID == TASK_SEND_DATA_TOKEN)
        port = config.clientTokenPort;
    qDebug() << QString("Task %1 created successfully for player %2").arg(taskID).arg(player.id) + QString::fromStdString(player.name + " at " + player.ip);
}


void CTask::run()
{
    // make connection and send data token to the player
    socket = new QTcpSocket();

    connect(socket, SIGNAL(connected()), this, SLOT(slotConnected()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(slotDisconnected()));
    connect(socket, SIGNAL(readyRead()), this, SLOT(slotReadyRead()));

    // try to connect to the player
    QHostAddress hostAddress(QString::fromStdString(player.ip));
    socket->connectToHost(hostAddress, this->port);

    // wait for response
    if (!socket->waitForConnected(config->waitTime)) {
        qDebug() << QString("[Error] Socket to player %1 :").arg(player.id) << socket->errorString();
        emit signalAppendText(QString("<font color=\"#FF0000\">[错误] 连接超时 Socket to player %1 ,").arg(QString::fromStdString(player.name + ',' + player.ip+':'+std::to_string(this->port))) + socket->errorString() + "</font>");
        emit signalChangeLabel(player.id, QString("<font color=red>[错误]连接超时</font>"));
    }
}

// send data token to a player
void CTask::slotSendToken()
{
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

    // 单位ms  每ms都发数据,发送速率跟不上; 所以,每次将tokenDuration这么长时间的token一起发送; 再用定时器触发下一个tokenDuration时间
    int tokenDuration = 10;

    int writeState = 1;
    int initTokenIdx = (this->tokenIdx) + 1;

    // 如果token还没有发送完,设置下一次触发的定时器
    if (initTokenIdx < this->dataTokens->size()-1 && (!this->needStop)) {
        int nextDeltaTime = this->dataTokens->at(initTokenIdx+1).timeStamp - this->dataTokens->at(initTokenIdx).timeStamp;
        this->timer->setInterval(std::max(tokenDuration, nextDeltaTime));
    }
    // 发送token
    double totData = this->dataTokens->size();
    while (this->dataTokens->at(this->tokenIdx + 1).timeStamp - this->dataTokens->at(initTokenIdx).timeStamp <= tokenDuration) {
        ++ this->tokenIdx;
        QByteArray cmdDataToken;
        QDataStream tokenStream(&cmdDataToken, QIODevice::WriteOnly);
        tokenStream << uchar(0x02) << this->dataTokens->at(this->tokenIdx).timeStamp;
        tokenStream << this->dataTokens->at(this->tokenIdx).type << this->dataTokens->at(this->tokenIdx).token << uchar(0x03);
        writeState = std::min(writeState, int(this->socket->write(cmdDataToken)));
        this->socket->flush();
        qDebug() << QString("[%1%] Sending token %2 (type:%3) to player ")
                    .arg(QString::number(this->tokenIdx/totData*100, 'f', 2))
                    .arg(this->dataTokens->at(this->tokenIdx).timeStamp)
                    .arg(QChar(this->dataTokens->at(this->tokenIdx).type))
                    + QString::number(player.id) + ' : ' + QString::fromStdString(player.name);
        if (this->tokenIdx == this->dataTokens->size()-1) break;
    }
    // 判断是否发送成功
    if (writeState > 0) {
//        emit signalAppendText(QString("[%1%] 发送时间戳%2的数据令牌至")
//                              .arg(QString::number(this->tokenIdx/totData*100, 'f',2))
//                              .arg(this->dataTokens->at(this->tokenIdx).timeStamp)
//                              + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip));
        emit signalChangeProgress(this->player.id, int(this->tokenIdx/totData*100));
        // 在QLabel中显示信息
        emit signalChangeLabel(player.id, QString("考试进度: 时间戳%1").arg(this->dataTokens->at(this->tokenIdx).timeStamp));
    }
    else {
        qDebug() << QString("[Error] Failed when sending token %1 to player ")
                    .arg(this->dataTokens->at(this->tokenIdx).timeStamp)
                    + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip);
        emit signalAppendText(QString("<font color=\"#FF0000\">[失败] 发送数据令牌%1至")
                              .arg(this->dataTokens->at(this->tokenIdx).timeStamp)
                              + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip) + "</font>");
        emit signalChangeProgress(this->player.id, -1);
        timer->stop();
        socket->disconnectFromHost();
        // 在QLabel中显示错误信息
        emit signalChangeLabel(player.id, QString("<font color=red>令牌%1发送出错:连接中断</font>").arg(this->dataTokens->at(this->tokenIdx).timeStamp));
    }

    // the last timestamp, stop send token task.
    if (this->tokenIdx >= this->dataTokens->size()-1 || this->needStop) {
        timer->stop();
        socket->disconnectFromHost();
        qDebug() << QString("[%1%] Send over! to player ")
                    .arg(QString::number(this->tokenIdx/totData*100, 'f', 2))
                    + QString::number(player.id) + ' : ' + QString::fromStdString(player.name);
        emit signalAppendText(QString("[%1%] <font color=\"#00B400\">数据令牌发送完成</font>，to ")
                              .arg(QString::number(this->tokenIdx/totData*100, 'f',2))
                              + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip));
        if (!this->needStop) {
            emit signalChangeProgress(this->player.id, int(100));
        }
        emit signalChangeLabel(player.id, QString("数据令牌发送完成"));
    }
}

// connected to the player, send command "Prepare competition"
void CTask::slotConnected()
{
    qDebug() << "Socket connected to player " + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip);
    emit signalAppendText(QString("连接到") + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip));

    // send command "Prepare competition"
    if (this->taskID == TASK_PREPARE_COMPETITION){
        const auto cmd = QByteArray::fromRawData(cmdPrepareCompetition, sizeof(cmdPrepareCompetition));
        int writeState = socket->write(cmd);
        socket->flush();
        if (writeState > 0) {
            qDebug() << "Sending 'Prepare competition.' to player " + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip);
            emit signalAppendText(QString("<font color=\"#00B400\">发送“准备考试”指令至") + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip)+"</font>");
        }
        else {
            qDebug() << "[Error] Failed when sending 'Prepare competition.' to player " + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip);
            emit signalAppendText(QString("<font color=\"#FF0000\">[失败] 发送“准备考试”指令至") + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip) + "</font>");
            emit signalChangeLabel(player.id, QString("<font color=red>[失败]'准备考试'发送超时</font>"));
        }
        socket->disconnectFromHost();
    }
    // send all data tokens by timestamp
    else if (this->taskID == TASK_SEND_DATA_TOKEN) {
        timer.reset(new QTimer(this));
        connect(timer.get(), SIGNAL(timeout()), this, SLOT(slotSendToken()));
        this->tokenIdx = -1;
        timer->start(100);  // invoke every 100ms (default)
    }
    else if (this->taskID == TASK_END_COMPETITION) {
        const auto cmd = QByteArray::fromRawData(cmdCompetitionOver, sizeof(cmdCompetitionOver));
        int writeState = socket->write(cmd);
        socket->flush();
        if (writeState > 0) {
            qDebug() << "Sending 'End competition.' to player " + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip);
            emit signalAppendText(QString("<font color=\"#00B400\">发送“结束考试”指令至") + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip)+"</font>");
        }
        else {
            qDebug() << "[Error] Failed when sending 'Prepare competition.' to player " + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip);
            emit signalAppendText(QString("<font color=\"#FF0000\">[失败] 发送“结束考试”指令至") + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip) + "</font>");
            emit signalChangeLabel(player.id, QString("<font color=red>[失败]'结束考试'发送超时</font>"));
        }
        socket->disconnectFromHost();
    }
    else if (this->taskID == TASK_START_COMPETITION) {
        const auto cmd = QByteArray::fromRawData(cmdStartCompetition, sizeof(cmdStartCompetition));
        int writeState = socket->write(cmd);
        socket->flush();
        if (writeState > 0) {
            qDebug() << "Sending 'Start competition.' to player " + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip);
            emit signalAppendText(QString("<font color=\"#00B400\">发送“开始考试”指令至") + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip)+"</font>");
        }
        else {
            qDebug() << "[Error] Failed when sending 'Start competition.' to player " + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip);
            emit signalAppendText(QString("<font color=\"#FF0000\">[失败] 发送“开始考试”指令至") + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip) + "</font>");
            emit signalChangeLabel(player.id, QString("<font color=red>[失败]'开始考试'发送超时</font>"));
        }
        socket->disconnectFromHost();
    }
}

// If receive message, check it (expected "Prepared")
void CTask::slotReadyRead()
{
    qDebug() << "Reading from player " + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip);
    emit signalAppendText("Reading from player " + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip));
    // read the data from the socket
    qDebug() << socket->readAll();
}

void CTask::slotDisconnected()
{
    qDebug() << "Disconnected with player " + QString::number(player.id)+ ' : ' + QString::fromStdString(player.name + " at " + player.ip);
//    emit signalAppendText("与" + QString::number(player.id) + ":" + QString::fromStdString(player.name) + "断开连接");
}

#ifndef CTASK_H
#define CTASK_H

#include <QObject>
#include <QThread>
#include <QTcpSocket>
#include <QMutex>
#include "header.h"
#include "cconfig.h"

class CTask : public QThread
{
    Q_OBJECT
public:
    explicit CTask();
    explicit CTask(CPlayer, CConfig &, std::vector<CDataToken> &, int,
                   Ui::MainWindowDesign *ui, QObject *parent = 0);
    // make connection and send data token to the player
    void run();
    bool needStop=false;

signals:
    void signalAppendText(const QString &text);
    void signalChangeProgress(const int idx, const int progress);
    void signalChangeLabel(const int id, const QString &text);

public slots:
    void slotConnected();
    void slotDisconnected();
    void slotReadyRead();
    void slotSendToken();

protected:

private:
    QTcpSocket *socket;
    CPlayer player;
    std::shared_ptr<CConfig> config;
    std::shared_ptr<std::vector<CDataToken>> dataTokens;
    quint16 port;
    int taskID;
    int tokenIdx=0;
    std::shared_ptr<QTimer> timer;
    Ui::MainWindowDesign *ui;
};

#endif // CTASK_H

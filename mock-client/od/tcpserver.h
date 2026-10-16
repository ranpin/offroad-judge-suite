#ifndef TCPSERVER_H
#define TCPSERVER_H
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <iostream>
#include <QVector>
#include <QReadWriteLock>

class TCPServer:public QThread
{
    Q_OBJECT
public:
    TCPServer(int,int, QObject *parent = 0);
    ~TCPServer();
    void run();
    int readData(char*&,int &);
    int readFixedData(QByteArray &);
    int getmessagelen(void);
    QReadWriteLock lock;

public slots:
    void newConnect();
    void readMessage();

private:
    QTcpServer *m_tcpserver;
    QTcpSocket *m_tcpsocket;
    QByteArray Fixeddata;
    QVector<QByteArray> dataVector;
    QVector<QByteArray> FixeddataVector;

    int connected;
    int lengthOfOneReam;
    int PORT;
    int totallen=0;

};

#endif // TCPSERVER_H

#ifndef TCPCLIENT_H
#define TCPCLIENT_H
#include <QtNetwork>
#include <QTcpSocket>
#include <iostream>

class TCPClient
{
public:
    TCPClient(QHostAddress,int);
    ~TCPClient();
    void writeData(char *, int);
    void connectServer();
    void closeConnect();

private:
    QTcpSocket *m_tcpsocket;
    QHostAddress Address;
    int Port;

};

#endif // TCPCLIENT_H

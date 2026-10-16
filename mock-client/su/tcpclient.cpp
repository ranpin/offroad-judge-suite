#include "tcpclient.h"

TCPClient::TCPClient(QHostAddress address,int port)
{
    Address = address;
    Port = port;
    //m_tcpsocket->connectToHost(Address,Port);
}

TCPClient::~TCPClient()
{
    delete m_tcpsocket;
}

void TCPClient::writeData(char * data, int size)
{
    //QByteArray datagram;
    //datagram.resize(size);
    //memcpy(datagram.data(),data,size);

//    if(size!=(m_tcpsocket->write(data,size)))
//    {
//        std::cout<<"Error! Write "<<size<<" bytes to socket.";
//    }
    m_tcpsocket->write(data,size);
    m_tcpsocket->waitForBytesWritten();
}

void TCPClient::connectServer()
{
    m_tcpsocket = new QTcpSocket;
    m_tcpsocket->connectToHost(Address,Port);
    if(!m_tcpsocket->waitForConnected(5000000))
    {
        std::cout<< "TCP socket connect to server failed! Server Port :"<< Port <<std::endl;
    }
    else
        std::cout<< "TCP socket connect to server succeed! Server Port:"<<Port<< std::endl;

}

void TCPClient::closeConnect()
{
    m_tcpsocket->disconnectFromHost();
    m_tcpsocket->waitForDisconnected();
    delete m_tcpsocket;
}

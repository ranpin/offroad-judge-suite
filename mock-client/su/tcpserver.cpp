#include "tcpserver.h"

TCPServer::TCPServer(int port,int length,QObject *parent):QThread(parent)
{
    PORT = port;
    lengthOfOneReam = length;
}

TCPServer::~TCPServer()
{
    m_tcpserver->close();
    m_tcpserver->deleteLater();
    delete m_tcpserver;
    delete m_tcpsocket;
}

void TCPServer::run()
{
    m_tcpserver = new QTcpServer;
    m_tcpsocket = new QTcpSocket;
    m_tcpserver->listen(QHostAddress::Any,PORT);
    connect(m_tcpserver,SIGNAL(newConnection()),this,SLOT(newConnect()),Qt::DirectConnection);
    QThread::exec();
    /*
    while(1)
    {
        m_tcpserver->waitForNewConnection(1000000);
       // if(m_tcpserver->hasPendingConnections())
        {
            std::cout<<"here is a new connect"<<std::endl;
            m_tcpsocket = m_tcpserver->nextPendingConnection();
            //std::cout<<m_tcpsocket->readBufferSize()<<std::endl;
            while(1)
            {
                m_tcpsocket->waitForReadyRead(1000000);
                std::cout<<"go"<<std::endl;
                QByteArray datagram;
                datagram = m_tcpsocket->readAll();
                std::cout<<"Receive : "<<datagram.data()<<std::endl;
            }

        }

    }
    */
    //connect(m_tcpserver,SIGNAL(newConnection()),this,SLOT(newConnect()));
}

void TCPServer::newConnect()
{
    std::cout<<"Local server has a new TCP connect at Port "<<PORT<<std::endl;
    m_tcpsocket = m_tcpserver->nextPendingConnection();
    connect(m_tcpsocket,SIGNAL(readyRead()),this,SLOT(readMessage()),Qt::DirectConnection);
    connect(m_tcpsocket,SIGNAL(disconnected()),m_tcpsocket,SLOT(deleteLater()),Qt::DirectConnection);
}

void TCPServer::readMessage()
{
    QByteArray datagram;
    datagram = m_tcpsocket->readAll();
    Fixeddata += datagram;
    totallen += datagram.size();
    QByteArray temp;
    while(Fixeddata.length()>=lengthOfOneReam)
    {

        temp = Fixeddata.mid(0,lengthOfOneReam);
        Fixeddata.remove(0,lengthOfOneReam);
        lock.lockForWrite();
        FixeddataVector.push_back(temp);
        lock.unlock();
    }
    char check;
    check = '\x02';
    dataVector.push_back(datagram);
//    //std::cout<< (int)datagram.at(0)<<std::endl;
//    if(dataVector.length()==1000)
//    {
//        for(int i =0;i<1000;i++)
//        {
//            for(int j=0;j<dataVector.at(i).size();j++)
//            {
//               std::cout<<(int)dataVector.at(i).at(j)<<"#";
//            }


//        }

//    }
    //std::cout<<"Receive : "<<datagram.data()<<std::endl;
}


int TCPServer::readData(char* &address,int & size)
{
    if(dataVector.isEmpty())
        return 0;
    size = dataVector.at(0).size();
    address = new char[size];
    memcpy(address,dataVector.at(0).data(),size);
    dataVector.pop_front();
    return 1;
}

int TCPServer::readFixedData(QByteArray &data)
{
    if(FixeddataVector.isEmpty())
        return 0;
    lock.lockForRead();
    data = FixeddataVector.at(0);
    lock.unlock();
    lock.lockForWrite();
    FixeddataVector.pop_front();
    lock.unlock();
    return 1;

}

int TCPServer::getmessagelen(void)
{
    return FixeddataVector.size();
}

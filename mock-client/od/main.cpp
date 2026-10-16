#include <QCoreApplication>
#include "tcpclient.h"
#include "tcpserver.h"
#include <cstring>
#include <cstdint>
#include <fstream>
#include<string>
#include<sstream>
#include <reverse.hpp>
#include <opencv2/opencv.hpp>
#include <cstdlib>
#include "CropImage.h"
#include <iostream>
#include "groundtruthfile.h"
#include <chrono>
#define min(a,b) ((a<b)?a:b)
#define max(a,b) ((a>b)?a:b)
//#include<Windows.h>//use it only for Sleep()
#define random(a,b) (rand()%(b-a+1)+a)
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
high_resolution_clock::time_point beginTime, endTime;
milliseconds timeInterval;


int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    std::string path_of_answer="./data/exams/EXAM-OD-001/Raw-001-Groundtruth/gt_detection.csv";
    GroundTruthFile gtfile(path_of_answer,true);
    GroundTruthData gtdata;

    loadData();
    char isrand;
    do{
    std::cout<<"If generate random data? [y/n] :"<<std::endl;
    std::cin>>isrand;}while((isrand!='y')&&(isrand!='n'));

    int Maxdelay;
    std::cout<<"Input max delay time (ms). Input 0 if don't want delay. MaxDelay = "<<std::endl;
    std::cin>>Maxdelay;


    TCPServer server1(7001,3);
    server1.start();

    TCPServer server2(7002,12);
    server2.start();
    std::cout<<server2.getmessagelen()<<std::endl;
    char *data;
    QByteArray signal;
    std::cout<<"Waiting for start signal......"<<std::endl;
    while(!server1.readFixedData(signal));
    if((signal.at(0)=='\x02')&&(signal.at(1)=='\x30')&&(signal.at(2)=='\x03'))
        std::cout<<"Receive start signal."<<std::endl;
    else{
        std::cout<<"start signal is wrong, exit program. "<<std::endl;
        return 0;
    }


    data = new char[3];
    data[0] = '\x02'; data[1] = '\x32'; data[2] = '\x03';

    QHostAddress addre("127.0.0.1");
    addre = QHostAddress::LocalHost;
    TCPClient client1(addre,7000);
    client1.connectServer();
    client1.writeData(data,3);
    //client1.closeConnect();


    TCPClient client2(addre,7003);
    client2.connectServer();
    //client2.closeConnect();

    std::cout<<"Send start single to server. "<<std::endl;

    char box_type_char;
    char attribute_char;
    int8_t box_type;
    int8_t attribute;
    double box[4][2];
    double longitude;
    double latitude;
    long long timeStamp_gt,last_timeStamp;
    int64_t timeStamp_tk;
    char typeOfToken;
    int valueoftoken;
    gtfile.ReadOneGTData(gtdata);
    timeStamp_gt = gtdata.timeStamp_gt;

    last_timeStamp = 0;
    QByteArray oneDatagram;
    //Debug point
    while(!server1.readFixedData(oneDatagram));
//    int finishflag = 0;
    int answercnt = 0;
    int finishflag =0;
    int sendingendflag = 0;
    std::ofstream infile("token_log.csv");
    while(1)
    {
//        std::cout<<server2.getmessagelen()<<std::endl;


        if (server1.readFixedData(signal))
            {
                sendingendflag = 1;
//                break;
            }


        while(!server2.readFixedData(oneDatagram));//等待数据到来


        char ddd[] ={oneDatagram.at(8),oneDatagram.at(7),oneDatagram.at(6),oneDatagram.at(5),oneDatagram.at(4),oneDatagram.at(3),oneDatagram.at(2),oneDatagram.at(1)};
        memcpy(&timeStamp_tk,ddd,8);
        typeOfToken = oneDatagram.at(9);
        valueoftoken = oneDatagram.at(10);

        std::cout<<timeStamp_tk<< ","<< typeOfToken <<"," << valueoftoken<< endl;
        infile<<timeStamp_tk<< ","<< typeOfToken <<"," << valueoftoken<< endl;
//        std::cout<<timeStamp_tk<<endl;

        if(timeStamp_tk<timeStamp_gt)continue;
//        std::cout<<server2.getmessagelen()<<std::endl;

        while(timeStamp_tk>=timeStamp_gt && !finishflag)
        {

        // 读入包围框类型
        box_type_char = gtdata.box_type_char;

        //for Debug

        box_type = (box_type_char == 'C') ? 0 : 1;


        // 读入目标属性
        attribute_char = gtdata.attribute_char;
        attribute = (attribute_char == 'H') ? 0 :(( attribute_char == 'V')?1:2);


        // 读入包围框顶点坐标
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 2; j++) {
                box[i][j] = gtdata.box[i][j];
            }
        }

        int x0 = 0, y0 = 0 ,image_length = 0;
        uint16_t width = 0, height = 0;
        char *image_data = new char;
/*      20180813 boxtype"=='C'"->"==0"*/
        std::cout<<"box_type="<<box_type_char;
        if(box_type == 0)
        {
            x0 = box[0][0];
            y0 = box[0][1];

            width = min(box[1][0],1023) - box[0][0] + 1 ;
            height = min(box[2][1],767) - box[1][1] + 1;
            if(isrand == 'y')
            {
                x0 = abs(x0 - random(0, 2*width));
                y0 = abs(y0 - random(0, 2*height));
                if(x0+width>1024)
                    x0 = 1023-width;
                if(y0+height >768)
                    y0 = 767-height;
                box[0][0] = x0;
                box[0][1] = y0;
                box[1][0] = x0+width-1;
                box[1][1] = y0;
                box[2][0] = x0+width-1;
                box[2][1] = y0+height-1;
                box[3][0] = x0;
                box[3][1] = y0+height-1;
            }

            cv::Mat roi;/*(width, height, CV_8UC3, cv::Scalar(0, 255, 255));*/

//            cv::Mat iroi;
//            cv::Rect irect(x0,y0,width,height);
//            bool f = cropImage(timeStamp_tk, irect, iroi);
//            cv::imwrite("testin.jpg",iroi);
            cv::Rect rect;
//            if (width<37||height<37)
//            {   int xlb = min(x0,1024+37-x0);
//                int xrb = min(1024-x0,(37-width)/2);
//                int yub = min(y0,768 +37-y0);
//                int ylb = min(768-y0,(37-height)/2);


//                rect=cv::Rect(x0 -xlb, y0 - yub,37,37);}

//            else
            rect=cv::Rect(x0,y0,width,height);
            /*************************************/
//            std::cout<<"x0 = "<<x0<<",y0 = "<<y0<<",width = "<<width<<",height = "<<height<<endl;
            bool flag = cropImage(gtdata.timeStamp_gt, rect, roi);

//            delete image_data;
//            image_data = new char[roi.rows*roi.cols*3 + 1];
//            enCodingImage(roi, image_data, image_length);
            std::string roihead = "roi" + to_string(answercnt) + "_" +to_string(gtdata.timeStamp_gt) +".jpg";
//            cv::imwrite(roihead,roi);

//            cv::Mat roitestout(roi.rows,roi.cols,CV_8UC3,cv::Scalar(0, 255, 255));
//            deCodingImage(image_data,roi.rows,roi.cols,roitestout);
//            cv::imwrite("test.jpg",roitestout);
            /************** resize ver****************************************/

            cv::Mat tans;
            if (width<50||height<50)
            {cv::resize(roi,tans,cv::Size(50,50));width=50;height=50;
                std::string resizehead = "resize" + to_string(answercnt)+"_" + to_string(gtdata.timeStamp_gt) +".jpg";
//                cv::imwrite(resizehead,tans);
                delete image_data;
                image_data = new char[tans.rows*tans.cols*3 + 1];
                enCodingImage(tans, image_data, image_length);}
            else if(width>200||height>200)
            {cv::resize(roi,tans,cv::Size(200,200));width=200;height=200;
                std::string resizehead = "resize" + to_string(answercnt)+"_" + to_string(gtdata.timeStamp_gt) +".jpg";
//                cv::imwrite(resizehead,tans);
                delete image_data;
                image_data = new char[tans.rows*tans.cols*3 + 1];
                enCodingImage(tans, image_data, image_length);}
            else
            {
                delete image_data;
                image_data = new char[roi.rows*roi.cols*3 + 1];
                enCodingImage(roi, image_data, image_length);
            }




////            cv::imshow("OUT",testout);
//            QString roipath = QString::number(gtdata.timeStamp_gt);


            /* *****************************************/
//            std::cout<<strlen(image_data)<<endl;
//            cv::Mat testout(tans.rows,tans.cols,CV_8UC3,cv::Scalar(0, 255, 255));
//            deCodingImage(image_data,tans.rows,tans.cols,testout);
////            cv::imshow("OUT",testout);
//            QString path = QString::number(gtdata.timeStamp_gt);

//            cv::imwrite("test",testout);
            /**************************************/
//            std::cout<<",image length = "<<image_length<<endl;
        }




        // 读取目标位置精度
        longitude = gtdata.longitude;

        // 读取目标位置纬度
        latitude = gtdata.latitude;
//        std::cout<<" "<<gtdata.timeStamp_gt<<std::endl;

        /* 编码答案数据 */
        uint32_t length_of_ODanswer = 95 + image_length;
        char *ODanswer = new char[101 + image_length];
        char start_of_socket = '\x02';
        char end_of_socket = '\x03';
        char type_of_ODanswer = '\x4F';
        uint16_t image_cols = width;
        uint16_t image_rows = height;

        long long curgttime = gtdata.timeStamp_gt;

        std::cout<< curgttime<<" "<<box_type<<" ";
                        std::cout<<"box = "<<box[0][0]<<", "<<box[0][1]<<", "<<box[3][0]<<", "<<box[3][1]<<" ";
        std::cout << attribute<<" "<<longitude<<" "<<latitude<<" ";
        std::cout << image_cols <<" "<<image_rows<<endl;


        length_of_ODanswer = revert_32(length_of_ODanswer);
        curgttime = revert_l(curgttime);


        for(int i = 0;i<4;i++)
        {
            box[i][0] = revert_d(box[i][0]);
            box[i][1] = revert_d(box[i][1]);
        }
        longitude = revert_d(longitude);
        latitude = revert_d(latitude);
        image_cols = revert_w(image_cols);
        image_rows = revert_w(image_rows);




        memcpy(ODanswer, &start_of_socket, 1);
        memcpy(ODanswer + 1, &length_of_ODanswer, 4);
        memcpy(ODanswer + 5, &type_of_ODanswer, 1);
        memcpy(ODanswer + 6, &curgttime, 8);
        memcpy(ODanswer + 14, &box_type, 1);
        memcpy(ODanswer + 15, box, 64);
        memcpy(ODanswer + 79, &attribute,1);
        //20180813   Exchange"lat"&"lon"in GTfile class
        memcpy(ODanswer + 80, &longitude, 8);
        memcpy(ODanswer + 88, &latitude, 8);
        memcpy(ODanswer + 96, &image_cols, 2);
        memcpy(ODanswer + 98, &image_rows, 2);
        if(image_length!=0)
        memcpy(ODanswer + 100, image_data, image_length);
        memcpy(ODanswer + 100 +image_length  , &end_of_socket, 1);

        std::cout<<" Answer "<<answercnt<<" : total length = "<< 95 +image_length<<" end = ";
        printf("%d",ODanswer[100+image_length]);
        cout<<"\n";
        answercnt ++;


//        if(Maxdelay>0)
//            Sleep(random(0,Maxdelay));

        //Debug Point
        client2.writeData(ODanswer,101+image_length);

        if(!gtfile.ReadOneGTData(gtdata))
        {   finishflag =1;
            break;}
        // 读入时间戳
        last_timeStamp = timeStamp_gt;
        timeStamp_gt = gtdata.timeStamp_gt;
        delete image_data;
        delete ODanswer;
        }
        if(finishflag==1)
            break;
    }
    infile.close();
    if (!(sendingendflag ==1))
        while(!server1.readFixedData(signal));
//    Sleep(5000);
    {std::cout<<"Received end signal. Send end signal to server......"<<std::endl;
    data[0] = '\x02'; data[1] = '\x35'; data[2] = '\x03';
    //client1.connectServer();
    client1.writeData(data,3);
    //client1.closeConnect();
    delete data;}


    return a.exec();
}

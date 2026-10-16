#include <QCoreApplication>
#include "tcpclient.h"
#include "tcpserver.h"
#include <cstring>
#include <cstdint>
#include <fstream>
#include <reverse.hpp>
#include <cstdlib>
#include <stdio.h>
//#include <io.h>
//#include <direct.h>
#include <algorithm>
#include <map>
//#include <windows.h>
#define random(a,b) (rand()%(b-a+1)+a)

struct locans{
    double lat;
    double lon;
    double head;
};
//#define path_of_answer "C:/Users/admin/Desktop/Testdata-001-Groundtruth/gt_detection.csv"
using namespace std;
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    std::string main_path = "./data/exams/EXAM-TM-001/Raw-001-Groundtruth/";
    std::string task_path = "./data/exams/EXAM-TM-001/Raw-001-Task.txt";
    std::string su_gt_path = "files/";
    std::string global_gt_path = "gt_global.csv";
    std::string loc_gt_path = "gt_local.csv";

    ifstream task_loader;
    int locnum,sunum;
    task_loader.open(task_path, std::ios::in);

    task_loader>>locnum;
    std::vector<long long> locrec;
    for (int i=0;i<locnum;i++)
    {
        long long tmp;
        task_loader>>tmp;
        locrec.push_back(tmp);
    }
    task_loader>>sunum;

    std::vector<long long> surec;
    for (int i=0;i<sunum;i++)
    {
        long long tmp;
        task_loader>>tmp;
        surec.push_back(tmp);
    }


    ifstream loc_loader;
    loc_loader.open(main_path+loc_gt_path,std::ios::in);
    std::string skipped_str;
    char skipped_char;
    loc_loader>>skipped_str;
    loc_loader>>skipped_str;
    loc_loader>>skipped_str;
    loc_loader>>skipped_str;
    std::map<long long,locans> loc;
    for (int i=0;i<locnum;i++)
    {
        long long tmp;
        locans tloc;
        loc_loader>>tmp;
        loc_loader>>skipped_char;
        loc_loader>>tloc.lat;
        loc_loader>>skipped_char;
        loc_loader>>tloc.lon;
        loc_loader>>skipped_char;
        loc_loader>>tloc.head;


        loc.insert(std::map<long long, locans>::value_type(tmp,tloc));
    }



    char judge;
    do{
    cout<<"If generate random data? [y/n] :"<<endl;
    cin>>judge;}while((judge!='y')&&(judge!='n'));


    TCPServer server1(7001,3);
    server1.start();

    TCPServer server2(7002,12);
    server2.start();


    int anscnt = 0;
    int gtcnt =  locrec.size()+surec.size();
    std::cout<<gtcnt<<std::endl;
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

    TCPClient client1(QHostAddress::LocalHost,7000);
    client1.connectServer();
    client1.writeData(data,3);
    //client1.closeConnect();


    TCPClient client2(QHostAddress::LocalHost,7003);
    client2.connectServer();
    //client2.writeData(data,3);
    //client2.closeConnect();



    std::cout<<"Send start single to server. "<<std::endl;


    QByteArray oneDatagram;
    while(!server1.readFixedData(signal));

    int num;
    int currIndex=0;
    int x,y;
    int8_t value;
    long long timeStamp_tk;
    char typeOfToken;
    int valueoftoken;
    ifstream file;

    char start_of_socket = '\x02';
    char end_of_socket = '\x03';
    int8_t EnvirMap[81*81];
    char answer_su[1+4+1+8+81*81+1];
    uint32_t length_su = 81*81+8+1;
    char type_of_answer_su = 'M';


    char answer_loc[1+4+1+8+8+8+8+1];

    uint32_t length_loc = 32+1;
    char type_of_answer_loc = 'L';



    std::ofstream infile("token_log.csv");

    while(1)
    {

        while(!server2.readFixedData(oneDatagram));//等待数据到来

        char ddd[] ={oneDatagram.at(8),oneDatagram.at(7),oneDatagram.at(6),oneDatagram.at(5),oneDatagram.at(4),oneDatagram.at(3),oneDatagram.at(2),oneDatagram.at(1)};
        memcpy(&timeStamp_tk,ddd,8);
        typeOfToken = oneDatagram.at(9);
        valueoftoken = oneDatagram.at(10);

        std::cout<<timeStamp_tk<< ","<< typeOfToken <<"," << valueoftoken<< endl;
        infile<<timeStamp_tk<< ","<< typeOfToken <<"," << valueoftoken<< endl;
        if(find(locrec.begin(),locrec.end(),timeStamp_tk)!=locrec.end())
        {
            cout<<"Send loc "<<timeStamp_tk<<" "<<endl;
            locans curloc = loc[timeStamp_tk];
            uint32_t curlen = revert_32(length_loc);
            long long curtime = revert_l(timeStamp_tk);
            double lat = revert_d(curloc.lat);
            double lon = revert_d(curloc.lon);
            double head = revert_d(curloc.head);
            memcpy(answer_loc,&start_of_socket,1);
            memcpy(answer_loc+1,&curlen,4);
            memcpy(answer_loc+5,&type_of_answer_loc,1);
            memcpy(answer_loc+6,&curtime,8);
            memcpy(answer_loc+14,&lon,8);
            memcpy(answer_loc+22,&lat,8);
            memcpy(answer_loc+30,&head,8);
            memcpy(answer_loc+38,&end_of_socket,1);
            client2.writeData(answer_loc,1+4+1+8+24+1);
            locrec.erase(find(locrec.begin(),locrec.end(),timeStamp_tk));
            anscnt ++;
        }
        if(find(surec.begin(),surec.end(),timeStamp_tk)!=surec.end())
        {
            cout<<"Send file "<<timeStamp_tk<<".csv"<<endl;
            file.open(main_path+su_gt_path+ to_string(timeStamp_tk)+".csv" ,std::ios_base::in);
                    if(!file.good())
                    {
                        cout<<"Groundtruth file open failed, exit program...\n";
                        getchar();
                        return 0;
                    }
                    std::string tmp;
                    char skipped_char;
                    file>>tmp>>tmp>>tmp;
                    while(!file.eof())
                    {
                        file>>x>>skipped_char>>y>>skipped_char>>value;
                        if(judge == 'n')
                            EnvirMap[x*81+y] = value - 48;
                        else
                        {
                             EnvirMap[x*81+y] = random(0,3);
                        }
                    }
                    file.close();
            uint32_t curlen = revert_32(length_su);
            long long curtime = revert_l(timeStamp_tk);
            memcpy(answer_su,&start_of_socket,1);
            memcpy(answer_su+1,&curlen,4);
            memcpy(answer_su+5,&type_of_answer_su,1);
            memcpy(answer_su+6,&curtime,8);
            memcpy(answer_su+14,EnvirMap,81*81);
            memcpy(answer_su+14+81*81,&end_of_socket,1);
            client2.writeData(answer_su,1+4+1+8+81*81+1);
            surec.erase(find(surec.begin(),surec.end(),timeStamp_tk));
            for(int i=0;i<81*81;i++)
                EnvirMap[i] = 0;
            anscnt++;


            //        /*20180813 length->4*/

            //        client2.writeData(answer,1+4+1+8+81*81+1);
            //        currIndex +=1;
            //        if(currIndex == 4)
            //            break;
        }

        if(anscnt == gtcnt)
            break;
//        cout<<"Send file "<<timestamp_gt[currIndex]<<".txt"<<endl;
//        file.open(root_direct+ to_string(timestamp_gt[currIndex])+".txt" ,std::ios_base::in);
//        if(!file.good())
//        {
//            cout<<"Groundtruth file open failed, exit program...\n";
//            getchar();
//            return 0;
//        }
//        file>>num;
//        for(int i=0;i<num;i++)
//        {
//            file>>x>>y>>value;
//            if(judge == 'n')
//                EnvirMap[x*81+y] = value - 48;
//            else
//            {
//                EnvirMap[x*81+y] = random(0,3);
//            }
//        }
//        file.close();

//        length = revert_w(length);
//        timeStamp_tk = revert_l(timeStamp_tk);

////        memcpy(answer,&start_of_socket,1);
////        memcpy(answer+1,&length,2);
////        memcpy(answer+3,&type_of_answer,1);
////        memcpy(answer+4,&timeStamp_tk,8);
////        memcpy(answer+12,EnvirMap,81*81);
////        memcpy(answer+12+81*81,&end_of_socket,1);
////        client2.writeData(answer,1+2+1+8+81*81+1);
//        /*20180813 length->4*/
//        memcpy(answer,&start_of_socket,1);
//        memcpy(answer+1,&length,4);
//        memcpy(answer+5,&type_of_answer,1);
//        memcpy(answer+6,&timeStamp_tk,8);
//        memcpy(answer+14,EnvirMap,81*81);
//        memcpy(answer+14+81*81,&end_of_socket,1);
//        client2.writeData(answer,1+4+1+8+81*81+1);
//        currIndex +=1;
//        if(currIndex == 4)
//            break;
    }
    infile.close();
    while(!server1.readFixedData(signal));


    std::cout<<"Received end signal. Send end signal to server......"<<std::endl;
    data[0] = '\x02'; data[1] = '\x35'; data[2] = '\x03';


    //client1.closeConnect();


    //Gv.txt
    const char* foldname="/media/exam";

//    if ( access(foldname,0) )
//        mkdir(foldname);



    ofstream Answer;

    ifstream global_loader;
    double tmplat,tmplon;
    global_loader.open(main_path+global_gt_path,std::ios::in);
    global_loader>>skipped_str;
    global_loader>>skipped_str;
    global_loader>>skipped_str;
    global_loader>>skipped_str;
    Answer.open("/media/exam/tmp/gv.txt", std::ios::out );  //以写入和在文件末尾添加的方式打开.txt文件，没有的话就创建该文件。
       if (!Answer.is_open())
        {std::cout<<"gv.txt open failed"<<std::endl;
           system("pause");
           return 0;}
    int   globalcnt =0;
    vector<double> latvec,lonvec;
    while(!global_loader.eof())
    {
        global_loader>>tmplat;
        latvec.push_back(tmplat);
        global_loader>>skipped_char;
        global_loader>>tmplon;
        lonvec.push_back(tmplon);
        global_loader>>skipped_char;
        global_loader>>skipped_str;
        global_loader>>skipped_str;
        globalcnt++;
    }



       Answer << globalcnt << endl;
       Answer.setf(ios::fixed,ios::floatfield);
       Answer.precision(7);
       for(int i=0;i<globalcnt;i++)

            Answer << i+1 <<" "<<latvec.at(i)<<" "<<lonvec.at(i)<< endl;


       Answer.close();

//    Sleep(5000);
    client1.connectServer();
    client1.writeData(data,3);





    delete data;


    return a.exec();
}

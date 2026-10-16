#include "groundtruthfile.h"

GroundTruthFile::GroundTruthFile(string name,bool ifsort)
{
    filename = name;
    file.open(filename);
    if(!file.good())
    {
        cout<<"Cannot open "<<filename<<", please put the file in the same path with program, exit program...\n";
        exit(-2);
    }
    cout<<"Open "<<filename<<" successfully!\n";
    char skipped_char; // 用于保存逗号
    int skipped_int; // 用于保存整数
    long long skipped_long; // 用于保存long long
    GroundTruthData tmpGTData;
    cnt = 0;
    while(!file.eof())
    {
        file >> skipped_int;
        if(file.eof())
            break;
        file >> skipped_char;
        if(file.eof())
            break;
        file >> tmpGTData.timeStamp_gt;
        file >> skipped_char;
        file >> skipped_long;
        file >> skipped_char;
        // 读入包围框类型
        file >> tmpGTData.box_type_char;
        file >> skipped_char;
        // 读入目标属性
        file >> tmpGTData.attribute_char;
        file >> skipped_char;
        file >> skipped_char;
        file >> skipped_char;
        // 读入包围框顶点坐标
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 2; j++) {
                file >> tmpGTData.box[i][j];
                file >> skipped_char;
            }
        }
        file >> skipped_char;
        file >> skipped_char;
        file >> skipped_char;
        file >> skipped_char;

        // 读取目标位置精度
        file >> tmpGTData.latitude;

        file >> skipped_char;

        // 读取目标位置纬度
        file >> tmpGTData.longitude;

        file >> skipped_char;
        file >> skipped_char;
        file >> skipped_char;
        file >> skipped_int;
        file >> skipped_char;
        file >> skipped_long;
        file >> skipped_char;
        file >> skipped_long;

        //cout<<tmpGTData.timeStamp_gt<<endl;
        if(!ifsort)
        {
            GTdata.push_back(tmpGTData);

        }
        else
        {
            if(GTdata.empty())
                GTdata.push_back(tmpGTData);
            else
            {
                vector<GroundTruthData>::iterator it;
                for(int i=0;i<GTdata.size()+1;i++)
                {
                    if(i==GTdata.size())
                    {
                        GTdata.push_back(tmpGTData);
                        break;
                    }
                    if(tmpGTData.timeStamp_gt<=GTdata.at(i).timeStamp_gt)
                    {
                        //cout<<GTdata.at(i).timeStamp_gt<<endl;
                        it = GTdata.begin()+i;
                        GTdata.insert(it,1,tmpGTData);
                        break;
                    }

                }
            }

        }

    }

}




bool GroundTruthFile:: ReadOneGTData(GroundTruthData& gtdata)
{
    if (cnt == GTdata.size())
        return 0;
    gtdata = GTdata.at(cnt);
    cnt++;
    return 1;
}




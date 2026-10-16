#ifndef GROUNDTRUTHFILE_H
#define GROUNDTRUTHFILE_H
#include <string>
#include <vector>
#include <fstream>
#include<iostream>
using namespace std;


class GroundTruthData{

public:
    long long timeStamp_gt;
    char box_type_char;
    char attribute_char;
    double box[4][2];
    double longitude;
    double latitude;
};



class GroundTruthFile
{
public:
    GroundTruthFile(string,bool);
    bool ReadOneGTData(GroundTruthData&);

private:
    string filename;
    ifstream file;
    int cnt;
    vector<GroundTruthData> GTdata;



};

#endif // GROUNDTRUTHFILE_H

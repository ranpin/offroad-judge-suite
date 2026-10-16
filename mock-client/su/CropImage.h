#ifndef _CROPIMAGE_H
#define _CROPIMAGE_H

#include <opencv2/opencv.hpp>
#include <string>
#include <fstream>
#include <queue>
#include <iostream>

class OneRecorder{
public:
	long long timeStamp;
	std::string filename;
};

bool loadData();

bool cropImage(long long timeStamp, cv::Rect rect,cv::Mat& ROI);

bool enCodingImage(const cv::Mat& src, char* data, int& length);

bool deCodingImage(char* data, int rows, int cols, cv::Mat& dst);


#endif

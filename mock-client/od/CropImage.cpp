#include "CropImage.h"

std::queue<OneRecorder> recorders;
std::string picture_path;

bool loadData(){
	std::string timeStamp_path;
    cv::FileStorage fs("config.yml", cv::FileStorage::READ);
    if(fs.isOpened())
        std::cout<<"Open config.yml successfully!"<<std::endl;
    else
    {
        std::cout<<"Cannot open config.yml, exit program... "<<std::endl;
        getchar();
        exit(-1);
    }
	fs["timeStamp_path"] >> timeStamp_path;
	fs["picture_path"] >> picture_path;
	fs.release();
	std::ifstream fp(timeStamp_path);
    if(!fp.good())
    {
        std::cout<<"Cannot open timestamp.csv, exit program... "<<std::endl;
        getchar();
        exit(-1);
    }
    else
        std::cout<<"Open timestamp.csv successfully ! "<<std::endl;


	fp >> timeStamp_path >> timeStamp_path;
	while (!fp.eof())
	{
		OneRecorder onerecorder;
		char comma;
		fp >> onerecorder.timeStamp;
		fp >> comma>>onerecorder.filename;
		recorders.push(onerecorder);
	}
	fp.close();
	return true;
}

bool cropImage(long long timeStamp, cv::Rect rect,cv::Mat& ROI){
	OneRecorder onerecorder;
	while (!recorders.empty())
	{
		onerecorder = recorders.front();

		if (timeStamp > onerecorder.timeStamp)
        {
            recorders.pop();
			continue;
        }
		else if (timeStamp == onerecorder.timeStamp)
			break;
		else
			return false;
	}
	if (recorders.empty()) return false;

	cv::Mat srcImage = cv::imread(picture_path + onerecorder.filename);
//    std::cout<<rect.x<<" "<<rect.y<<" "<<rect.width<<" "<<rect.height<<std::endl;
//    std::cout<<onerecorder.filename<<srcImage.cols<<" "<<srcImage.rows<<" "<<srcImage.size<<std::endl;
    ROI = srcImage(rect);


	return true;
}

bool enCodingImage(const cv::Mat& src, char* data, int& length){
	length = 0;
	int m = src.rows;
	int n = src.cols;
	for (int i = 0; i < m;i++)
	for (int j = 0; j < n; j++){
		data[length++] = src.at<cv::Vec3b>(i, j)[0]; //B
		data[length++] = src.at<cv::Vec3b>(i, j)[1]; //G
		data[length++] = src.at<cv::Vec3b>(i, j)[2]; //R
	}
	return true;
}

bool deCodingImage(char* data, int rows, int cols, cv::Mat& dst){
	int p = 0;
	for (int i = 0; i < rows;i++)
	for (int j = 0; j < cols; j++){
		dst.at<cv::Vec3b>(i, j)[0] = data[p++]; //B
		dst.at<cv::Vec3b>(i, j)[1] = data[p++]; //G
		dst.at<cv::Vec3b>(i, j)[2] = data[p++]; //R
	}
	return true;
}

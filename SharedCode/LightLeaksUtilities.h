#pragma once

#include "ofxCv.h"

using namespace ofxCv;
using namespace cv;

void medianThreshold(cv::Mat& src, float thresholdValue) {
	cv::Mat thresholded, filtered;
	ofxCv::threshold(src, thresholded, thresholdValue);
	ofxCv::medianBlur(thresholded, filtered, 3);
	thresholded &= filtered;
	min(src, thresholded, src);
}

void buildProMap(int proWidth, int proHeight,
								 const Mat& binaryCoded,
								 const Mat& camConfidence,
								 Mat& proConfidence,
								 Mat& proMap) {	
	int camWidth = camConfidence.cols, camHeight = camConfidence.rows;
	ofLogVerbose() << "building proMap";
	proMap = Mat::zeros(proHeight, proWidth, CV_16UC3);
	proConfidence = Mat::zeros(proHeight, proWidth, CV_32FC1);
	for(int cy = 0; cy < camHeight; cy++) {
		for(int cx = 0; cx < camWidth; cx++) {
			float curCamConfidence = camConfidence.at<float>(cy, cx);
			Vec3w pxy = binaryCoded.at<Vec3w>(cy, cx);
			unsigned short px = pxy[0], py = pxy[1];
			if(px < proWidth && py < proHeight) {
				Vec3w curProMap(cx, cy, 0);
				float curProConfidence = proConfidence.at<float>(py, px);
				if(curCamConfidence > curProConfidence) {
					proConfidence.at<float>(py, px) = curCamConfidence;
					proMap.at<Vec3w>(py, px) = curProMap;
				}
			}
		}
	}
	medianThreshold(proConfidence, .25);
}

void setCalibrationDataPathRoot(){
    ofSetDataPathRoot(ofToDataPath("",true)+"../../../SharedData/");

    return ;
}


vector<ofFile> getScanNames(){
    ofDirectory rootDir;
    rootDir.listDir(ofToDataPath("",true));
    
    vector<ofFile> ret;
    for(int i=0;i<rootDir.size();i++){
        if(rootDir.getPath(i)[0] != '_'){
            ret.push_back(ofFile(rootDir.getPath(i)));
        }
    }
    return ret;
}


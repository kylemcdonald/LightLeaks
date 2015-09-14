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


void buildProMapDist(int pw, int ph,
                     const Mat& binaryCodedIn,
                     const Mat& camConfidenceIn,
                     Mat& proConfidence,
                     Mat& proMap,
                     int k) {
    proMap = Mat::zeros(ph, pw, CV_16UC3);
    proConfidence = Mat::zeros(ph, pw, CV_32FC1);
    
    Mat camConfidence;
    Mat binaryCoded;
    cv::blur(camConfidenceIn, camConfidence, cv::Size(k, k));
    cv::blur(binaryCodedIn, binaryCoded, cv::Size(k, k));
    
    int n = 1 + k * 2;
    Mat1f weights = Mat1f::zeros(n, n);
    for(int y = 0; y < n; y++) {
        for(int x = 0; x < n; x++) {
            float dx = x - k, dy = y - k;
            float distance = sqrtf(dx * dx + dy * dy);
            weights(y, x) = (float) k / (k + distance); // falloff function
        }
    }
    int ch = camConfidence.rows;
    int cw = camConfidence.cols;
    for(int cy = 0; cy < ch; cy++) {
        for(int cx = 0; cx < cw; cx++) {
            const float& cconf = camConfidence.at<float>(cy, cx);
            const Vec3w& cmap = binaryCoded.at<Vec3w>(cy, cx);
            int px = cmap[0], py = cmap[1];
            if(px >= 0 && px < pw && py >= 0 && py < ph) {
                for(int oy = -k; oy <= +k; oy++) {
                    for(int ox = -k; ox <= +k; ox++) {
                        int pox = px + ox, poy = py + oy;
                        if(pox >= 0 && pox < pw && poy >= 0 && poy < ph) {
                            int wx = ox + k, wy = oy + k;
                            float& weight = weights(wy, wx);
                            float& pconf = proConfidence.at<float>(poy, pox);
                            float curconf = weight * cconf;
                            if(curconf > pconf) {
                                Vec3w& pmap = proMap.at<Vec3w>(poy, pox);
                                pmap(0) = cx;
                                pmap(1) = cy;
                                pconf = curconf;
                            }
                        }
                    }
                }
            }
        }
    }
    medianThreshold(proConfidence, .25);
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
            const Vec3w& pxy = binaryCoded.at<Vec3w>(cy, cx);
            const unsigned short& px = pxy[0], py = pxy[1];
            if(px < proWidth && py < proHeight) {
                Vec3w curProMap(cx, cy, 0);
                float& curProConfidence = proConfidence.at<float>(py, px);
                if(curCamConfidence > curProConfidence) {
                    curProConfidence = curCamConfidence;
                    proMap.at<Vec3w>(py, px) = curProMap;
                }
            }
        }
    }
//    medianThreshold(proConfidence, .25);
}

void setCalibrationDataPathRoot() {
    ofSetDataPathRoot(ofToDataPath("") + "/../../../SharedData");
    return ;
}


vector<ofFile> getScanNames(){
    ofDirectory rootDir;
    rootDir.listDir(ofToDataPath("",true));
    
    vector<ofFile> ret;
    for(int i=0;i<rootDir.size();i++){
        if(rootDir.getName(i)[0] != '_' && rootDir.getName(i)[0] != '.'){
            ret.push_back(ofFile(rootDir.getPath(i)));
        }
    }
    return ret;
}


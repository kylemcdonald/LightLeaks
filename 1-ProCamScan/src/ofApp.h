// a better confidence metric also accounts for agreement between levels?

#pragma once

#include "ofMain.h"
#include "ofxCv.h"
#include "ofxProCamToolkit.h"

class ofApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();

    ofDirectory cameraMasksDir;
    ofDirectory dirHorizontalNormal, dirHorizontalInverse;
    ofDirectory dirVerticalNormal, dirVerticalInverse;
    vector<ofFile> hnFiles, hiFiles, vnFiles, viFiles;
    vector<ofImage*> hnImageNormal, hnImageInverse, viImageNormal, viImageInverse;
    
    int horizontalBits, verticalBits, camWidth, camHeight, proWidth, proHeight, proCount;
    cv::Mat camConfidence, binaryCodedHorizontal, binaryCodedVertical, minImage, maxImage;
    ofImage cameraMask, projectorMask;
    
    cv::Mat proConfidence, proMap;
    
    ofxCv::Calibration calibration;
    int time;

    void processImageSet(ofFile fileNormal, ofFile fileInverse, ofImage *& imageNormal, ofImage *& imageInverse, const cv::Mat cameraMask, cv::Mat referenceImage, string name);

};

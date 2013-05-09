// a better confidence metric also accounts for agreement between levels?

#pragma once

#include "ofMain.h"
#include "ofxCv.h"
#include "ofxProCamToolkit.h"

class testApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();

	ofDirectory dirHorizontalNormal, dirHorizontalInverse;
	ofDirectory dirVerticalNormal, dirVerticalInverse;
	vector<ofFile> hnFiles, hiFiles, vnFiles, viFiles;
	int horizontalBits, verticalBits, camWidth, camHeight, proWidth, proHeight;
	cv::Mat camConfidence, binaryCodedHorizontal, binaryCodedVertical;
	
	cv::Mat proMap, proConfidence, totalGuesses;
	cv::Mat minImage, maxImage;
	ofImage cameraMask;
};

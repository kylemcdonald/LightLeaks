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
	int n, camWidth, camHeight, proWidth, proHeight;
	cv::Mat confidence, binaryCodedHorizontal, binaryCodedVertical;
};

#pragma once

#include "ofMain.h"
#include "ofxCv.h"

class ofApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
	
	ofShortPixels binaryCodedPix;
	ofFloatPixels camConfidencePix;
	ofPixels minImagePix, maxImagePix;
	cv::Mat proConfidence, proMap, mean, stddev, count;
};
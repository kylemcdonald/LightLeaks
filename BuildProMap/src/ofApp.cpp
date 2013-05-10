#include "ofApp.h"

#include "LightLeaksUtilities.h"

using namespace ofxCv;
using namespace cv;

void ofApp::setup() {
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	
	//int proWidth = 1024, proHeight = 768;
	//int proWidth = 3 * 512, proHeight = 256;
	int proWidth = 1024*3, proHeight = 768;
	
	ofLoadImage(binaryCodedPix, "binaryCoded.png");
	ofLoadImage(camConfidencePix, "camConfidence.exr");
	
	buildProMap(proWidth, proHeight,
							binaryCodedPix,
							camConfidencePix,
							proConfidence,
							proMap,
							mean,
							stddev,
							count);
	
	saveImage(proConfidence, "proConfidence.exr");
	saveImage(proMap, "proMap.png");
	saveImage(mean, "mean.png");
	saveImage(stddev, "stddev.exr");
	saveImage(count, "count.png");
}

void ofApp::update() {
}

void ofApp::draw() {
}
#include "ofApp.h"

#include "LightLeaksUtilities.h"

using namespace ofxCv;
using namespace cv;

void ofApp::setup() {
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	ofSetLogLevel(OF_LOG_VERBOSE);
	
	//int proWidth = 1024, proHeight = 768;
	//int proWidth = 3 * 512, proHeight = 256;
	int proWidth = 1024*3, proHeight = 768;
	
	ofLoadImage(binaryCodedPix, "binaryCoded.png");
	ofLoadImage(camConfidencePix, "camConfidence.exr");
	Mat binaryCoded = toCv(binaryCodedPix);
	Mat camConfidence = toCv(camConfidencePix);
	
	buildProMap(proWidth, proHeight,
							binaryCoded,
							camConfidence,
							proConfidence,
							proMap);
	
	saveImage(proConfidence, "proConfidence.exr");
	saveImage(proMap, "proMap.png");
}

void ofApp::update() {
}

void ofApp::draw() {
}
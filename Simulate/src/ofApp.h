#pragma once

#include "ofMain.h"
#include "ofxCv.h"

class ofApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
	
	ofShortPixels proMap;
    ofImage maskImage;
	ofFloatPixels xyzMap, proConfidence;
	ofVboMesh mesh;
	ofEasyCam cam;
	vector<string> directories;
};
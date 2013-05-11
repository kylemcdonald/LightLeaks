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
	vector<ofVboMesh> mesh;
    vector<bool> drawMesh;
	ofEasyCam cam;
	vector<string> directories;
    
    void keyPressed(int key);
};
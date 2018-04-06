#pragma once

#include "ofMain.h"
#include "ofAutoShader.h"
#include "ofAutoImage.h"

class ofApp : public ofBaseApp {
public:
    void setup();
    void update();
	void draw();
	void keyPressed(int key);
	
	ofAutoImage<float> xyzMap;
    ofAutoImage<float> confidenceMap;
    ofAutoImage<unsigned char> calibrationMap;
	ofAutoShader shader;
};

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
	
	ofAutoImage<float> proConfidence;
    ofAutoImage<unsigned short> proMap;
	ofAutoShader shader;
};

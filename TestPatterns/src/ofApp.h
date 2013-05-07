#pragma once

#include "ofMain.h"
#include "ofAutoShader.h"

class ofApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
	
	void keyPressed(int key);
	
	ofAutoShader shader;
};
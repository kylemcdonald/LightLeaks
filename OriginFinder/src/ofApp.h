#pragma once

#include "ofMain.h"
#include "ofxCv.h"

class ofApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
	
	void mousePressed(int x, int y, int button);
	void mouseDragged(int x, int y, int button);
	
	ofShortImage binaryCoded, proMap;
	ofFloatImage proConfidence, camConfidence;
	int startx, starty, offsetx, offsety;
};
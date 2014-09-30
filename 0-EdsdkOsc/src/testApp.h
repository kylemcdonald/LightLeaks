#pragma once

#include "ofMain.h"

#include "ofxEdsdk.h"
#include "ofxOsc.h"

class testApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
	
	void keyPressed(int key);
	
    ofxEdsdk::Camera camera;
    ofImage preview;
	ofxOscReceiver oscIn;
	ofxOscSender oscOut;
	
	string savePath;
	bool capturing;
	bool manual;
    
    string remoteComputer;
};

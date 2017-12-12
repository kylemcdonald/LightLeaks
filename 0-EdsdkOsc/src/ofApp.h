#pragma once

#include "ofMain.h"

#include "ofxEdsdk.h"
#include "ofxOsc.h"

class ofApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
    void exit();
	
	void keyPressed(int key);
	
    ofxEdsdk::Camera camera;
    ofImage preview;
	ofxOscReceiver oscIn;
    ofxOscSender oscOutPrimary, oscOutSecondary;
	
	string savePath;
	bool capturing;
	bool manual;
    
    string remoteComputerPrimary, remoteComputerSecondary;
};

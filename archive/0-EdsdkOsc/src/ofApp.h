#pragma once

#include "ofMain.h"

#include "ofxEdsdk.h"
#include "ofxWebServer.h"

class ofApp : public ofBaseApp, public ofxWSRequestHandler {
public:
	void setup();
	void update();
	void draw();
    void exit();
    
	void keyPressed(int key);
	
    ofxEdsdk::Camera camera;
    ofImage preview;
//	ofxOscReceiver oscIn;
//    ofxOscSender oscOutPrimary, oscOutSecondary;
    ofxWebServer server;
    void httpGet(string url);


	
	string savePath;
	bool capturing;
	bool manual;
    
    
//    string remoteComputerPrimary, remoteComputerSecondary;
};

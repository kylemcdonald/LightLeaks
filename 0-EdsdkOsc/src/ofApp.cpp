#include "ofApp.h"

void ofApp::setup() {
	ofXml settings;
	settings.load("../../../SharedData/settings.xml");
	
    remoteComputer = settings.getValue("osc/projector");
    
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	ofSetLogLevel(OF_LOG_VERBOSE);
	oscIn.setup(9000);
	oscOut.setup(remoteComputer, 9001);
	manual = false;
	camera.setup();
}

void ofApp::exit() {
    camera.close();
}

void ofApp::update() {
    camera.update();
    
    ofxOscMessage msgIn;
    while(oscIn.getNextMessage(msgIn)) {
        if(msgIn.getAddress() == "/takeAndSavePhoto") {
            savePath = msgIn.getArgAsString(0);
            ofLog() << "Taking and saving photo to " << savePath;
			camera.takePhoto();
		}
	}
	
	if(camera.isPhotoNew()) {
        if(manual) {
            manual = false;
            savePath = "out.jpg";
        } else {
			ofxOscMessage msgOut;
            msgOut.setAddress("/newPhoto");
            msgOut.addStringArg(savePath);
			oscOut.sendMessage(msgOut);
        }
        camera.savePhoto(savePath);
        preview.load(savePath);
	}
}

void ofApp::draw() {
	ofBackground(0);
	ofSetColor(255);
	
    if(preview.isAllocated()){
        preview.draw(0, 0, ofGetWidth(), ofGetHeight());
    }
    
	ofDrawBitmapStringHighlight("savePath: " + savePath, 10, 20);
    ofDrawBitmapStringHighlight("remote computer: " + remoteComputer, 10, 40);
    ofDrawBitmapStringHighlight("press 'p' to take a preview image", 10, 60);
    ofDrawBitmapStringHighlight("press ' ' to start the scan", 10, 80);
    ofDrawBitmapStringHighlight("press 'tab' to continue broken communication", 10, 100);
}

void ofApp::keyPressed(int key) {
    if(key == 'p') {
        manual = true;
		camera.takePhoto();
	}
    if(key == 'f') {
        ofToggleFullscreen();
    }
    if(key == ' ') {
        ofxOscMessage msgOut;
        msgOut.setAddress("/start");
        oscOut.sendMessage(msgOut);
    }
    if(key == '\t') {
        ofxOscMessage msgOut;
        msgOut.setAddress("/newPhoto");
        // in this case, lastPath is empty, and the same pattern is re-shot
        oscOut.sendMessage(msgOut);
    }
}
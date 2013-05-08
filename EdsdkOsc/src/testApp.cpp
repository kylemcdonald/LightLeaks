#include "testApp.h"

void testApp::setup() {
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	ofHideCursor();
	ofSetLogLevel(OF_LOG_VERBOSE);
	oscIn.setup(9000);
	oscOut.setup("disco.local", 9001);
	capturing = false;
	camera.setup();
}

void testApp::update() {
	ofxOscMessage msgIn;
	while(oscIn.getNextMessage(&msgIn)) {
		if(msgIn.getAddress() == "/takePhoto") {
			camera.takePhoto();
			capturing = true;
		}
		if(msgIn.getAddress() == "/savePhoto") {
			string filename = msgIn.getArgAsString(0);
			camera.savePhoto(filename);
		}
		if(msgIn.getAddress() == "/update") {
			camera.update();
		}
		if(msgIn.getAddress() == "/setup") {
			camera.setup();
		}
		if(msgIn.getAddress() == "/createDirectory") {
			string directory = msgIn.getArgAsString(0);
			ofDirectory::createDirectory(directory, true, true);
		}
	}
	
	if(camera.isPhotoNew()) {
		ofxOscMessage msgOut;
		msgOut.setAddress("/newPhoto");
		oscOut.sendMessage(msgOut);
	}
	
	ofxOscMessage msgOut;
	msgOut.setAddress("/capturing");
	if(!capturing) {
		msgOut.addIntArg(0);
	} else {
		msgOut.addIntArg(1);
	}
	oscOut.sendMessage(msgOut);
}

void testApp::draw() {
	ofBackground(0);
	ofSetColor(255);
	if(!capturing) {
		camera.draw(0, 0);
	}
	ofDrawBitmapStringHighlight("savePath: " + savePath, 10, 20);
	ofDrawBitmapStringHighlight("capturing: " + ofToString(capturing), 10, 40);
}
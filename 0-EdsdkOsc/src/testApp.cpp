#include "testApp.h"

void testApp::setup() {
	ofXml settings;
	settings.load("../../../SharedData/settings.xml");
	
    remoteComputer = settings.getValue("osc/camera");
    
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	ofSetLogLevel(OF_LOG_VERBOSE);
	oscIn.setup(9000);
	oscOut.setup(remoteComputer, 9001);
	capturing = false;
	manual = false;
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
			savePath = msgIn.getArgAsString(0);
			camera.savePhoto(savePath);
            preview.loadImage(savePath);
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
		if(manual) {
            savePath = "out.jpg";
			camera.savePhoto(savePath);
			manual = false;
            preview.loadImage(savePath);

		} else {
			ofxOscMessage msgOut;
			msgOut.setAddress("/newPhoto");
			oscOut.sendMessage(msgOut);
		}
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
	
    if(preview.isAllocated()){
        preview.draw(0, 0, ofGetWidth(), ofGetHeight());
    }
    
	ofDrawBitmapStringHighlight("savePath: " + savePath, 10, 20);
    ofDrawBitmapStringHighlight("remote computer: " + remoteComputer, 10, 40);
	ofDrawBitmapStringHighlight("capturing: " + ofToString(capturing), 10, 60);
    ofDrawBitmapStringHighlight("press space to take a preview image", 10, 80);

}

void testApp::keyPressed(int key) {
	if(key == ' ') {
		camera.takePhoto();
		manual = true;
	}
}
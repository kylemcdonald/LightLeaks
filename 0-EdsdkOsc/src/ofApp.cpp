#include "ofApp.h"

void ofApp::setup() {
	ofXml settings;
	settings.load("../../../SharedData/settings.xml");
	
//    remoteComputerPrimary = settings.getValue("osc/projector/primary");
//    remoteComputerSecondary = settings.getValue("osc/projector/secondary");
    
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	ofSetLogLevel(OF_LOG_VERBOSE);
//	oscIn.setup(9000);
//    oscOutPrimary.setup(remoteComputerPrimary, 9001);
//    oscOutSecondary.setup(remoteComputerSecondary, 9001);
	manual = false;

//    camera.setLiveView(false);
//	camera.setup();
    
    
    server.start("httpdocs", 8080);
    server.addHandler(this, "actions/*");
    
}

void ofApp::exit() {
//    camera.close();
}

void ofApp::httpGet(string url) {
    ofLog()<<"Get: "<<url;
    
    string actionTakePhoto = "/actions/takePhoto";
    if(url.compare(actionTakePhoto) >= 0){
        savePath = url.substr(actionTakePhoto.size());
        ofLog()<<"Take photo: "<<savePath;
        capturing = true;
        takePhotoSync();
        httpResponse("Photo taken");
        
    }
}

void ofApp::takePhotoSync(){
//    camera.takePhoto();
//    capturing = true;
//    while(capturing){
//        camera.update();
//        if(camera.isPhotoNew()){
//            capturing = false;
//            return;
//        }
//    }
}

void ofApp::update() {
    if(capturing){
        
        capturing = false;
    }
//    camera.update();
    
//    ofxOscMessage msgIn;
//    while(oscIn.getNextMessage(msgIn)) {
//        if(msgIn.getAddress() == "/takeAndSavePhoto") {
//            savePath = msgIn.getArgAsString(0);
//            ofLog() << "Taking and saving photo to " << savePath;
//			camera.takePhoto();
//		}
//	}
//	
//	if(camera.isPhotoNew()) {
//        if(manual) {
//            manual = false;
//            savePath = "out.jpg";
//        } else {
//			ofxOscMessage msgOut;
//            msgOut.setAddress("/newPhoto");
//            msgOut.addStringArg(savePath);
//            oscOutPrimary.sendMessage(msgOut);
//            oscOutSecondary.sendMessage(msgOut);
//        }
//        camera.savePhoto(savePath);
//        preview.load(savePath);
//	}
}

void ofApp::draw() {
	ofBackground(0);
	ofSetColor(255);
	
//    if(preview.isAllocated()){
//        preview.draw(0, 0, ofGetWidth(), ofGetHeight());
//    }
    
    ofDrawBitmapStringHighlight("savePath: " + savePath, 10, 20);
//    ofDrawBitmapStringHighlight("remote computer primary: " + remoteComputerPrimary, 10, 40);
//    ofDrawBitmapStringHighlight("remote computer secondary: " + remoteComputerSecondary, 10, 60);
    ofDrawBitmapStringHighlight("press 'p' to take a preview image", 10, 80);
    ofDrawBitmapStringHighlight("press ' ' to start the scan", 10, 100);
    ofDrawBitmapStringHighlight("press 'tab' to continue broken communication", 10, 120);
}

void ofApp::keyPressed(int key) {
    if(key == 'p') {
        manual = true;
//		camera.takePhoto();
	}
    if(key == 'f') {
        ofToggleFullscreen();
    }
    if(key == ' ') {
//        ofxOscMessage msgOut;
//        msgOut.setAddress("/start");
//        oscOutPrimary.sendMessage(msgOut);
//        oscOutSecondary.sendMessage(msgOut);
    }
    if(key == '\t') {
//        ofxOscMessage msgOut;
//        msgOut.setAddress("/newPhoto");
//        // in this case, lastPath is empty, and the same pattern is re-shot
//        oscOutPrimary.sendMessage(msgOut);
//        oscOutSecondary.sendMessage(msgOut);
    }
}

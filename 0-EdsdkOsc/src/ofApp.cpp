#include "ofApp.h"

void ofApp::setup() {
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	ofSetLogLevel(OF_LOG_VERBOSE);
	manual = false;

    camera.setLiveView(false);
	camera.setup();
    
    server.start("httpdocs", 8080);
    server.addHandler(this, "actions/*");
}

void ofApp::exit() {
    camera.close();
}

void ofApp::httpGet(string url) {
    ofLog()<<"HTTP action: "<<url;
    
    string actionTakePhoto = "/actions/takePhoto";
    if(url.compare(actionTakePhoto) >= 0){
        savePath = url.substr(actionTakePhoto.size()+1);
        if(savePath.size() == 0){
            savePath = "preview.jpg";
        }
        ofLog()<<"Take photo: "<<savePath;
        capturing = true;
        camera.takePhoto();
        while(!camera.isPhotoNew()){
            ofSleepMillis(30);
        }
        camera.savePhoto(savePath);
        
        httpResponse("Photo taken");
        
        dispatch_async(dispatch_get_main_queue(), ^{
            preview.load(savePath);
        });
    }
}

void ofApp::update() {
}

void ofApp::draw() {
	ofBackground(0);
	ofSetColor(255);
	
    if(preview.isAllocated()){
        preview.draw(0, 0, ofGetWidth(), ofGetHeight());
    }
    
    ofDrawBitmapStringHighlight("savePath: " + savePath, 10, 20);
    ofDrawBitmapStringHighlight("press 'p' to take a preview image", 10, 80);
//    ofDrawBitmapStringHighlight("press ' ' to start the scan", 10, 100);
//    ofDrawBitmapStringHighlight("press 'tab' to continue broken communication", 10, 120);
}

void ofApp::keyPressed(int key) {
    if(key == 'p') {
        camera.takePhoto();
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            while(!camera.isPhotoNew()){
                ofSleepMillis(30);
            }
            dispatch_async(dispatch_get_main_queue(), ^{
                camera.savePhoto("preview.jpg");
                preview.load("preview.jpg");
            });
        });
	}
    if(key == 'f') {
        ofToggleFullscreen();
    }
    if(key == ' ') {
    }
    if(key == '\t') {
    }
}

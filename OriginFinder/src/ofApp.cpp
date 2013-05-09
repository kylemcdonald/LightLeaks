#include "ofApp.h"

void ofApp::setup() {
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	
	binaryCoded.loadImage("binaryCoded.png");
	proMap.loadImage("proMap.png");
	proConfidence.loadImage("proConfidence.exr");
	camConfidence.loadImage("camConfidence.exr");
	
	offsetx = 0, offsety = 0;
}

void ofApp::update() {
}

void ofApp::draw() {
	ofSetColor(255);
	
	ofPushMatrix();
	ofTranslate(offsetx, offsety);
	camConfidence.draw(0, 0);
	ofPopMatrix();
	
	float scaleFactor = .5;
	
	ofPushMatrix();
	ofScale(scaleFactor, scaleFactor);
	proConfidence.draw(0, 0);
	ofPopMatrix();
	
	int selectx = mouseX - offsetx, selecty = mouseY - offsety;
	int radius = 2;
	for(int x = -radius; x < radius; x++) {
		for(int y = -radius; y < radius; y++) {
			ofShortColor cur = binaryCoded.getColor(ofClamp(selectx + x, 0, binaryCoded.getWidth()),
																							ofClamp(selecty + y, 0, binaryCoded.getHeight()));
			if(cur.r != 0 && cur.g != 0) {
				ofLine(mouseX, mouseY, cur.r * scaleFactor, cur.g * scaleFactor);		
			}
		}
	}
	ofNoFill();
	ofSetColor(ofColor::red);
	ofCircle(mouseX, mouseY, radius);
}

void ofApp::mousePressed(int x, int y, int button) {
	startx = x - offsetx, starty = y - offsety;
}

void ofApp::mouseDragged(int x, int y, int button) {
	offsetx = x - startx, offsety = y - starty;
}
#pragma once

#include "ofMain.h"

#include "ofxOsc.h"

class EdsdkOsc {
protected:
	ofxOscReceiver oscIn;
	ofxOscSender oscOut;
	bool newPhoto;
	int width, height;
    string lastPath;
    
	
	void sendMessage(string address) {
		ofxOscMessage msgOut;
		msgOut.setAddress(address);
		oscOut.sendMessage(msgOut);
	}
	
public:
    bool start;
    bool error;

	EdsdkOsc()
	:newPhoto(false),
	width(1056),
	height(704),
    start(false),
    lastPath(""),
    error(false){
	}
    
	void setup() {
		ofXml settings;
		settings.load("../../../SharedData/settings.xml");
		oscIn.setup(9001);
		oscOut.setup(settings.getValue("osc/camera"), 9000);
		ofAddListener(ofEvents().update, this, &EdsdkOsc::updateOsc);
		sendMessage("/setup");
	}
	void updateOsc(ofEventArgs &args) {
		ofxOscMessage msgIn;
		while(oscIn.getNextMessage(&msgIn)) {
			if(msgIn.getAddress() == "/newPhoto") {
                cout<<"Take photo receipt "<<endl;

				newPhoto = true;
                if(msgIn.getNumArgs() >= 1){
                    if(lastPath != msgIn.getArgAsString(0)){
                        error = true;
                        cout<<"Error path not correct "<<endl;
                    }
                } else {
                    error = true;
                    cout<<"Error path not set "<<endl;
                }
			}
            if(msgIn.getAddress() == "/start") {
                start = true;
            }
		}
	}
	void createDirectory(string directory) {
		ofxOscMessage msgOut;
		msgOut.setAddress("/createDirectory");
		msgOut.addStringArg(directory);
		oscOut.sendMessage(msgOut);
	}
	void update() {
		//sendMessage("/update");
	}
	void takePhoto(string filename, bool primary) {
        cout<<"Take photo "<<filename<<endl;
        error = false;
        lastPath = filename;
        if(primary) {
        ofxOscMessage msgOut;
            msgOut.setAddress("/takeAndSavePhoto");
            msgOut.addStringArg(filename);
            oscOut.sendMessage(msgOut);
        }
	}
	
	void draw(float x, float y) {
		ofPushMatrix();
		ofPushStyle();
		ofTranslate(x, y);
		ofFill();
		ofSetColor(0);
		ofRect(0, 0, width, height);
		ofNoFill();
		ofSetColor(255);
		ofRect(0, 0, width, height);
		ofLine(0, 0, width, height);
		ofLine(width, 0, 0, height);
		ofPopStyle();
		ofPopMatrix();
	}
	bool isPhotoNew() {
		bool prevNewPhoto = newPhoto;
		newPhoto = false;
		return prevNewPhoto;
	}
};

#include "ofApp.h"


float cubicEaseInOut(float time, float duration=1.0, float startValue = 0.0, float valueChange = 1.0){
    float t = time;
    float d = duration;
    float b = startValue;
    float c = valueChange;
    
    t /= d/2.;
    if (t < 1) return c/2.*t*t*t + b;
    t -= 2.;
    return c/2.*(t*t*t + 2.) + b;
}

void ofApp::setup() {
	ofSetLogLevel(OF_LOG_VERBOSE);
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	shader.setup("shader");
    
    
	xyzMap.loadImage("xyzMap.exr");
    normalMap.loadImage("normalMap.exr");
    confidenceMap.loadImage("confidenceMap.exr");
    
    stage = Lighthouse;
    
    intermezzoTimer = 10;

	//ofHideCursor();
}

void ofApp::update() {
    float dt = 1./ofGetFrameRate();
    
    stageAge += MIN(dt,0.1);
    
    //Go to intermezzo now and then
    if(stage != Intermezzo){
        intermezzoTimer -= dt;
        
        if(intermezzoTimer < 0){
            stageGoal = Intermezzo;
            intermezzoTimer = 10;
        }
    } else {
        //Go back to lighhouse after some seconds
        if(stageAge > 5){
            stageGoal = Lighthouse;
        }
    }
    
    
    if(stage == Lighthouse){
        lighthouseAngle += dt * cubicEaseInOut(stageAmp*0.7);
    }
    
    
    
    if(stage != stageGoal){
        stageAmp -= dt*0.5;
        if(stageAmp < 0){
            stage = stageGoal;
            stageAmp = 0;
            stageAge = 0;
        }
    } else {
        stageAmp = MIN(stageAmp+dt*0.5, 1.);
    }
	
    
}

void ofApp::draw() {
	ofBackground(0);
    
    //Lighthouse
    float beamWidth = 0;
    if(stage == Lighthouse){
        beamWidth = 0.3 * cubicEaseInOut(stageAmp);
    }
    
    
	shader.begin();
	shader.setUniform1f("elapsedTime", ofGetElapsedTimef());
    shader.setUniform1f("beamAngle", fmodf(lighthouseAngle, PI));
    shader.setUniform1f("beamWidth", beamWidth);
    shader.setUniform2f("spotlightPos", (float)ofGetMouseX() / ofGetWidth(), (float)ofGetMouseY()/ofGetHeight());
    shader.setUniform1f("stage", stage);
    
	shader.setUniformTexture("xyzMap", xyzMap, 0);
	shader.setUniformTexture("normalMap", normalMap, 2);
    shader.setUniformTexture("confidenceMap", confidenceMap, 3);
    
	xyzMap.draw(0, 0);
	shader.end();
    
    ofSetColor(255);
    ofDrawBitmapString("Stage "+ofToString(stage)+" goal "+ofToString(stageGoal)+"  amp "+ofToString(stageAmp), ofPoint(20,30));
}

void ofApp::keyPressed(int key) {
	if(key == ' ') {
        room = !room;
        
        if(room){
            xyzMap.loadImage("xyzMapRoom.exr");

        } else {
            xyzMap.loadImage("xyzMap.exr");
        }
	}
    if(key == 'f'){
        ofToggleFullscreen();
    }
}
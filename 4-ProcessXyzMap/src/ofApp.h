#pragma once

#include "ofMain.h"
#include "ofxAssimpModelLoader.h"
#include "ofxCv.h"
#include "ofAutoShader.h"

enum Stage {
    Lighthouse=0,
    Spotlight,
    Intermezzo
} ;

class ofApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
	void keyPressed(int key);
    
    bool debugMode;
	
	ofFloatImage xyzMap;
    ofFloatImage normalMap;
    ofFloatImage confidenceMap;
	ofAutoShader shader;
    
    Stage stage;
    Stage stageGoal;
    
    float stageAge;
    float stageAmp;

    //Lighthouse
    float lighthouseAngle;

    //Intermezzo
    float intermezzoTimer;
    
    
    
    //Speaker sampling
    ofFloatImage speakerXYZMap;
    ofFbo speakerFbo;
    ofFloatPixels speakerPixels;

    
    float speakerAmp[4];
    
    //Tracking
    ofVideoGrabber grabber;
};
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
	
	ofFloatImage xyzMap;
    ofFloatImage normalMap;
    ofFloatImage confidenceMap;
	ofAutoShader shader;
    
    ofImage img;
    
    bool room;
    
    Stage stage;
    Stage stageGoal;
    
    float stageAge;
    float stageAmp;

    //Lighthouse
    float lighthouseAngle;

    //Intermezzo
    float intermezzoTimer;

};
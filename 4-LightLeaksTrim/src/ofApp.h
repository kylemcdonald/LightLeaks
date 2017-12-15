#pragma once

#include "ofMain.h"
#include "ofAutoShader.h"

enum Stage {
    Lighthouse=0,
    Spotlight,
    Intermezzo,
    Linescan
};

class ofApp : public ofBaseApp {
public:
    void setup();
    void update();
	void draw();
    void exit();
	void keyPressed(int key);
    void mouseMoved(int x, int y);
    void mousePressed( int x, int y, int button );
    
    bool debugMode;
    int debugStage;
    
    
    float dt, previousTime;
	
	ofFloatImage xyzMap;
    ofFloatImage normalMap;
    ofFloatImage confidenceMap;
	ofShader shader;
    
    void startStage(Stage stage);
    Stage stage;
    Stage stageGoal;
    int substage;
    
    float stageAge;
    float stageAmp;

    //Lighthouse
    float lighthouseAngle;

    //Spotlight
    float spotlightThresholder;
    
    ofImage kl;
    
    //Scanlines
    int scanDir;
};

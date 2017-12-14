#include "ofApp.h"

const float trackScale = .25; // actual resizing on the image data
const float previewScale = .25; // presentation on the screen for calibration
const ofVec2f previewOffset(440, 248); // placement of debug image for calibration
const float backgroundLearningTime = 4; // in frames
const int foregroundBlur = 15; // after scaling by trackScale
const int foregroundDilate = 5; // connect disconnected parts. will fail to separate a big group of people.
const float contourFinderThreshold = 32; // higher blur calls for lower threshold
const float minAreaRadius = 5; // after scaling by trackScale
const float maxAreaRadius = 80; // after scaling by trackScale
const float lighthouseSpeed = 3;

const float durationIntermezzo = 30;
const float intervalIntermezzo = 30;
const int intermezzoCount = 5;
const float delaySpotlight = 1; // in and out delay
const int photoFrequency = 1; // every 1 spotlights

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

string getStageName(int stage) {
    switch(stage) {
        case 0: return "Lighthouse";
        case 1: return "Spotlight";
        case 2: return "Intermezzo";
        case 3: return "Linescan";
        default: return "Unknown";
    }
}


void ofApp::setup() {
    if(!setupCalled){

        setupCalled = true;
        ofSetLogLevel(OF_LOG_VERBOSE);
        
        previousTime = 0;
        
        //Shader
        shader.load("shader");
        
        xyzMap.load("../../SharedData/xyzMap.exr");
//        normalMap.load("../../../SharedData/normalMap.exr");
        confidenceMap.load("../../SharedData/confidenceMap.exr");
        
        xyzMap.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
//        normalMap.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        confidenceMap.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        
        stage = Lighthouse;
        substage = 0;
        
    }
    
    int numWindows = 1;
}

void ofApp::update() {
    
    if(ofGetFrameNum() % 60 == 0){
        shader.load("shader");
    }
    
    int numWindows = 1;

    
    float currentTime = ofGetElapsedTimef();
    dt = currentTime - previousTime;
    dt = ofClamp(dt, 0, .1);
    previousTime = currentTime;
    
    stageAge += dt;
    
    if(debugMode){
        if(debugStage == 0){
            stageGoal = Linescan;
            substage = scanDir;
        } else if(debugStage == 1){
            stageGoal = Lighthouse;
            substage = 0;
            lighthouseAngle = TWO_PI*mouseX/1920.0;
        }
        else if(debugStage == 2){
            stageGoal = Linescan;
            substage = 4;
        }
    } else {
        
        // if we're not on intermezzo
        if(stage != Intermezzo){
            // and enough time has passed since last stage change
            if(stageAge > intervalIntermezzo){
                // go to intermezzo
                stageGoal = Intermezzo;
            }
        }
        // if we're on intermezzo
        else {
            // but it's run its duration
            if(stageAge > durationIntermezzo){
                // and we're due for a spotlight
                if(spotlightThresholder == delaySpotlight){
                    // go to spotlight
                    stageGoal = Spotlight;
                }
                // if we're not due for a spotlight, return to lighthouse
                else {
                    // go to lighthouse
                    stageGoal = Lighthouse;
                }
            }
        }
        
        // if we're not already in spotlight
        
        if(stage == Lighthouse){
            lighthouseAngle += dt * cubicEaseInOut(stageAmp) * lighthouseSpeed;
        }
        
     //   stageGoal = Intermezzo;
        
    }
    
    if(stage != stageGoal){
        stageAmp -= dt*0.5;
        if(stageAmp <= 0){
            stageAmp = 0;
            stageAge = 0;
            stage = stageGoal;
            startStage(stage);
            if(stage == Intermezzo) {
                substage = (substage + 1) % intermezzoCount;
            }
        }
    } else {
        stageAmp = ofClamp(stageAmp+dt*0.5, 0, 1.);
    }
}

void ofApp::draw() {
    int numWindows = 1;
    
    ofBackground(0);
    ofEnableAlphaBlending();
    ofSetColor(255);
    
    //Lighthouse parameters
    float beamWidth = 0;
    if(stage == Lighthouse){
        // lighthouse is small most of the time, comes and goes from 0
        beamWidth = ofMap(cubicEaseInOut(stageAmp), 0, 1, 0, .3);
    }
    float spotlightSize = 0.2 * cubicEaseInOut(stageAmp);
    
    shader.begin(); {
        shader.setUniform1f("elapsedTime", ofGetElapsedTimef());
        shader.setUniform1f("beamAngle", fmodf(lighthouseAngle, TWO_PI));
        shader.setUniform1f("beamWidth", beamWidth);
        // convert where x is the long side of the room, y is short
        // into the model's coordinates where z is the long side, y is short
        shader.setUniform3f("spotlightPos",0,0,0);
        shader.setUniform1f("spotlightSize", spotlightSize);
        shader.setUniform1i("stage", stage);
        shader.setUniform1i("substage", substage);
        shader.setUniform1f("stageAmp", stageAmp);
        shader.setUniform2f("mouse", ofVec2f((float)mouseX/1920, (float)mouseY/ofGetHeight()));
        shader.setUniformTexture("xyzMap", xyzMap, 0);
        //shader.setUniformTexture("normalMap", normalMap, 2);
        shader.setUniformTexture("confidenceMap", confidenceMap, 3);
//        shader.setUniformTexture("kl", kl, 4);
        shader.setUniform1i("useConfidence", 1);
        
//        float curTime = ofGetElapsedTimef();
//        float timeSinceBeat = curTime - lastBeat;
        shader.setUniform1f("timeSinceBeat", 0);// timeSinceBeat);
        
        // DRaw all projectors in one window
        xyzMap.draw(0, 0, ofGetWidth(), ofGetHeight());
        
    } shader.end();
    
    
    //Debug text
    if(debugMode){
        ofSetColor(255);
        ofDrawBitmapStringHighlight(ofToString(ofGetFrameRate(), 0), 5, ofGetHeight() - 5);
        
        ofPushStyle();
        ofPushMatrix();
        ofTranslate(10, 120);
        ofDrawRectangle(0, 0, 4, stageAmp * 100);
        ofTranslate(14, 8);
        ofDrawBitmapStringHighlight(getStageName(stageGoal), 0, 0);
        ofDrawBitmapStringHighlight(getStageName(stage), 0, 100);
        ofPopMatrix();
        ofPopStyle();
    }
    
}

void ofApp::startStage(Stage stage) {
}

void ofApp::exit() {
}

void ofApp::keyPressed(int key) {
    if(key == 'f'){
        ofToggleFullscreen();
    }
    if(key == 'd'){
        debugMode = !debugMode;
    }
    if(key == 's'){
        shader.load("shader");
    }
    if(debugMode){
        if(key == 'x'){
            debugStage = 0;
            scanDir = 0;
        }
        if(key == 'y'){
            debugStage = 0;
            scanDir = 1;
        }
        if(key == 'z'){
            debugStage = 0;
            scanDir = 2;
        }
        if(key == 'l'){
            debugStage = 1;
        }
        if(key == 'c'){
            debugStage = 2;
        }
    }
}

void ofApp::mouseMoved(int x, int y){
}

void ofApp::mousePressed( int x, int y, int button ){
}

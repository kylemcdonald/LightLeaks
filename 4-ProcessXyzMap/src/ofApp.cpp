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
    
    //Create the speaker fbo
    ofVec3f speakers[4];
    speakers[0] = ofVec3f(0,0,0);
    speakers[1] = ofVec3f(1,0,0);
    speakers[2] = ofVec3f(1,1,0);
    speakers[3] = ofVec3f(0,1,0);
    
    

    
    speakerXYZMap.allocate(4, 100, OF_IMAGE_COLOR_ALPHA);
    
    float speakerAreaSize = 0.1;
    
    float * pixels = speakerXYZMap.getPixels();
    for(int y=0;y<speakerXYZMap.getHeight();y++){
        for(int x=0;x<speakerXYZMap.getWidth();x++){
            
            
            pixels[0] = speakers[x].x + sin(y * TWO_PI / 20) * ((float)y/speakerXYZMap.getHeight()) * speakerAreaSize;
            pixels[1] = speakers[x].y ;
            pixels[2] = speakers[x].z + cos(y * TWO_PI / 20) * ((float)y/speakerXYZMap.getHeight()) * speakerAreaSize;;
            pixels[3] = 1.0;
            pixels += 4;
        }
    }
    speakerXYZMap.update();
    
    speakerFbo.allocate(speakerXYZMap.getWidth(), speakerXYZMap.getHeight());
    speakerPixels.allocate(speakerXYZMap.getWidth(), speakerXYZMap.getHeight(),4);

    

	//ofHideCursor();
}

void ofApp::update() {
    float dt = 1./ofGetFrameRate();
    
    stageAge += MIN(dt,0.1);
    
    //Go to intermezzo now and then
  /*  if(stage != Intermezzo){
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
    */
    
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
    
    //Lighthouse parameters
    float beamWidth = 0;
    if(stage == Lighthouse){
        beamWidth = 0.3 * cubicEaseInOut(stageAmp);
    }
    
    
    shader.begin();{
        shader.setUniform1f("elapsedTime", ofGetElapsedTimef());
        shader.setUniform1f("beamAngle", fmodf(lighthouseAngle, PI));
        shader.setUniform1f("beamWidth", beamWidth);
        shader.setUniform2f("spotlightPos", (float)ofGetMouseX() / ofGetWidth(), (float)ofGetMouseY()/ofGetHeight());
        shader.setUniform1f("stage", stage);
        
        shader.setUniformTexture("xyzMap", xyzMap, 0);
        shader.setUniformTexture("normalMap", normalMap, 2);
        shader.setUniformTexture("confidenceMap", confidenceMap, 3);
        
        xyzMap.draw(0, 0);
    }shader.end();
    

    //Debug text
    ofSetColor(255);
    ofDrawBitmapString("Stage "+ofToString(stage)+" goal "+ofToString(stageGoal)+"  amp "+ofToString(stageAmp), ofPoint(20,30));
    
    
    //
    //Speaker sampling code
    //
    speakerFbo.begin(); {
        
        shader.begin();
        
        speakerXYZMap.draw(0,0);
        
        shader.end();
        
        
    } speakerFbo.end();


    //Read back the fbo, and avarage it on the CPU
    speakerFbo.getTextureReference().readToPixels(speakerPixels);
    
    for(int s=0;s<4;s++){
        speakerAmp[s] = 0;
        for(int y=0;y<speakerFbo.getHeight();y++){
            speakerAmp[s] += speakerPixels.getColor(s, y)[0];
        }
        speakerAmp[s] /= speakerFbo.getHeight();
    }
    
    //Debug drawing
    ofSetColor(255);
    speakerXYZMap.draw(0,0);
    speakerFbo.draw(10,0);


    //The simple scattering algorithm behind the speaker thingy
    /*  ofSetColor(255,0,0);
    for(int i=0;i<100;i++){
        float x = 100 + sin(i * TWO_PI / 20) * i;
        float y = 100 + cos(i * TWO_PI / 20) * i;
        
        ofCircle(x, y, 5);
        
    }*/

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
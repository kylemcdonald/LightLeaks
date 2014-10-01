#include "ofApp.h"

#define PREVIEW_SCALE (400./1920)


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
    ofEnableAlphaBlending();
    
    
    //Settings
    settings.load("settings.xml");
    
    debugMode = true;

    previousTime = 0;
    
    //Shader
	shader.setup("shader");
    
    
	xyzMap.loadImage("../../../SharedData/xyzMap.exr");
    normalMap.loadImage("../../../SharedData/normalMap.exr");
    confidenceMap.loadImage("../../../SharedData/confidenceMap.exr");
    
    stage = Lighthouse;
    
    //Spotlight setup
    spotlightPosition.setFc(0.01); //Low pass biquad filter - allow only slow frequencies
    
    //Intermezzo setup
    intermezzoTimer = 10;
    
    
    //Create the speaker fbo
    ofVec3f speakers[4];
    speakers[0] = ofVec3f(0,0,0);
    speakers[1] = ofVec3f(1,0,0);
    speakers[2] = ofVec3f(1,1,0);
    speakers[3] = ofVec3f(0,1,0);
    
    
    speakerXYZMap.allocate(4, 100, OF_IMAGE_COLOR_ALPHA);
    
    float speakerAreaSize = 0.2;
    
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

    
    
    //Tracker
    grabber.setup(1920, 1080, 25);
    

    
    contourFinder.setMinAreaRadius(20);
    contourFinder.setMaxAreaRadius(200);
    contourFinder.setUseTargetColor(false);
    
    
    cameraCalibrationCorners[0] = ofVec2f(settings.getValue("corner0x",0),
                                          settings.getValue("corner0y",0));
    cameraCalibrationCorners[1] = ofVec2f(settings.getValue("corner1x",1920),
                                          settings.getValue("corner1y",0));
    cameraCalibrationCorners[2] = ofVec2f(settings.getValue("corner2x",1920),
                                          settings.getValue("corner2y",1080));
    cameraCalibrationCorners[3] = ofVec2f(settings.getValue("corner3x",0),
                                          settings.getValue("corner3y",1080));
    
    setCorner = -1;
    firstFrame = true;
    
    updateCameraCalibration();
    
    
    oscSender.setup("localhost", 7777);
}

void ofApp::update() {
    float currentTime = ofGetElapsedTimef();
    float dt = currentTime - previousTime;
    dt = ofClamp(dt, 0, .1);
    previousTime = currentTime;
    
    stageAge += dt;
    
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
    
    //Go to spotlight when there are contours to track
    if(stage != Spotlight){
        if(spotlightThresholder > 1){
            stageGoal = Spotlight;
        }
    } else {
        //Go back to lighhouse after some seconds
        if(stageAge > 5){
            stageGoal = Lighthouse;
            spotlightThresholder = 0;
        }
    }
    
    if(stage == Lighthouse){
        lighthouseAngle += dt * cubicEaseInOut(stageAmp) * (3 + sin(ofGetElapsedTimeMillis() / 5000.));
    }
    
    
    
    if(stage != stageGoal){
        stageAmp -= dt*0.5;
        if(stageAmp < 0){
            stage = stageGoal;
            stageAmp = 0;
            stageAge = 0;
        }
    } else {
        stageAmp = ofClamp(stageAmp+dt*0.5, 0, 1.);
    }
    
    
    //Tracker
    bool newFrame = grabber.update();
    
    if(newFrame ) {
        ofPixels pixels = grabber.getGrayPixels();
        if(pixels.getWidth()>0){
            contourFinder.setThreshold(128);
            cv::Mat mat = ofxCv::toCv(pixels);

            cameraBackground.setDifferenceMode(ofxCv::RunningBackground::BRIGHTER);
            cameraBackground.update(mat, thresholdedImage);

            if(firstFrame){
                cameraBackground.reset();
            }
            
            
            ofxCv::blur(thresholdedImage, 5);
            thresholdedImage.update();
            

            contourFinder.findContours(thresholdedImage);
            
            if(contourFinder.getContours().size() > 0){
                ofRectangle rect =  ofxCv::toOf(contourFinder.getBoundingRect(0));
                ofVec2f point = ofVec2f(rect.x, rect.y+rect.height*0.5);
                spotlightPosition.update(cameraCalibration.inversetransform(point));
            }
            
            firstFrame = false;
        }
        
        if(contourFinder.getContours().size() > 0){
            spotlightThresholder += dt;
        } else {
            spotlightThresholder -= dt;
        }
    }
    
    
    //OSC
    ofxOscMessage msg;
    msg.setAddress("/audio/lighthouse_angle");
    msg.addFloatArg(fmodf(lighthouseAngle/TWO_PI, 1));
    oscSender.sendMessage(msg);
}

void ofApp::draw() {    
	ofBackground(0);
       ofEnableAlphaBlending();
    ofSetColor(255);
    
    //Lighthouse parameters
    float beamWidth = 0;
    if(stage == Lighthouse){
        beamWidth = 0.3 * cubicEaseInOut(stageAmp);
    }
    float spotlightSize = 0.1;
    if(stage == Spotlight){
        spotlightSize = 0.1 * cubicEaseInOut(stageAmp);
    }
    
    shader.begin();{
        shader.setUniform1f("elapsedTime", ofGetElapsedTimef());
        shader.setUniform1f("beamAngle", fmodf(lighthouseAngle, TWO_PI));
        shader.setUniform1f("beamWidth", beamWidth);
//        shader.setUniform2f("spotlightPos", (float)ofGetMouseX() / ofGetWidth(), (float)ofGetMouseY()/ofGetHeight());
        shader.setUniform2f("spotlightPos", (float) spotlightPosition.value().x, (float)spotlightPosition.value().y);
        shader.setUniform1f("spotlightSize", spotlightSize);
        
        shader.setUniform1i("stage", stage);
        
        shader.setUniformTexture("xyzMap", xyzMap, 0);
        shader.setUniformTexture("normalMap", normalMap, 2);
        shader.setUniformTexture("confidenceMap", confidenceMap, 3);
        shader.setUniform1i("useConfidence", 1);
        
        xyzMap.draw(0, 0);
    }shader.end();
    

    //Debug text
    if(debugMode){
        ofSetColor(0,100);
        ofRect(410, 0, 250, 100);
        ofSetColor(255);
        ofDrawBitmapString("Stage "+ofToString(stage)+" goal "+ofToString(stageGoal)+"  amp "+ofToString(stageAmp)+ " fps "+ofToString(ofGetFrameRate(),0), ofPoint(440,30));
    }
    
    
    //
    //Speaker sampling code
    //
    speakerFbo.begin(); {
        shader.begin();
        shader.setUniformTexture("xyzMap", speakerXYZMap, 0);
        shader.setUniform1i("useConfidence", 0);
        speakerXYZMap.draw(0,0);
        shader.end();
    } speakerFbo.end();


    //Read back the fbo, and average it on the CPU
    speakerFbo.getTextureReference().readToPixels(speakerPixels);
    
    for(int s=0;s<4;s++){
        speakerAmp[s] = 0;
        for(int y=0;y<speakerFbo.getHeight();y++){
            speakerAmp[s] += speakerPixels.getColor(s, y)[0];
        }
        speakerAmp[s] /= speakerFbo.getHeight();
    }
    
    //Debug drawing
    if(debugMode){
        ofPushMatrix();
        ofTranslate(420, 0);
        ofSetColor(255);
        speakerXYZMap.draw(0,0);
        speakerFbo.draw(10,0);
        
        ofDrawBitmapString("Speaker "+ofToString(speakerAmp[0])+" "+ofToString(speakerAmp[1])+" "+ofToString(speakerAmp[2])+" "+ofToString(speakerAmp[3]), ofPoint(20,45));
        ofPopMatrix();
    }


    //The simple scattering algorithm behind the speaker thingy visualized
    /*  ofSetColor(255,0,0);
    for(int i=0;i<100;i++){
        float x = 100 + sin(i * TWO_PI / 20) * i;
        float y = 100 + cos(i * TWO_PI / 20) * i;
        
        ofCircle(x, y, 5);
        
    }*/
    
    
    //Tracker
    if(debugMode){
        ofPushMatrix();
        ofTranslate(0, 300);
        ofScale(PREVIEW_SCALE, PREVIEW_SCALE);
        grabber.drawGray();
        contourFinder.draw();
        
        if(thresholdedImage.isAllocated()){
            thresholdedImage.draw(0,1080);
        }
        
        ofPushStyle();
        ofNoFill();
        ofSetColor(255, 0, 0);
        //        for(int i = 0; i < contourFinder.getContours().size(); i++) {
        { int i=0;
            if(contourFinder.getContours().size() > i){
                ofRectangle rect =  ofxCv::toOf(contourFinder.getBoundingRect(i));
                ofVec2f point = ofVec2f(rect.x, rect.y+rect.height*0.5);
             //   point = cameraCalibration.inversetransform(point);
                ofCircle(point.x, point.y, 20);
            }
        }
        
        ofSetColor(255, 255, 0);
        glBegin(GL_LINE_STRIP);
        for(int i=0;i<4;i++){
            glVertex2d(cameraCalibrationCorners[i].x, cameraCalibrationCorners[i].y);
        }
        glVertex2d(cameraCalibrationCorners[0].x, cameraCalibrationCorners[0].y);
        glEnd();
        ofPopStyle();

        ofPopMatrix();
        
        ofDrawBitmapString("Spotlight pos "+ofToString(spotlightPosition.value().x,1)+" "+ofToString(spotlightPosition.value().y,1), ofPoint(440,65));
    }

}


void ofApp::updateCameraCalibration(){
    ofVec2f inputCorners[4];
    inputCorners[0] = ofVec2f(0,0.5);
    inputCorners[1] = ofVec2f(1,0.5);
    inputCorners[2] = ofVec2f(1,0);
    inputCorners[3] = ofVec2f(0,0);
    
    cameraCalibration.calculateMatrix(inputCorners, cameraCalibrationCorners);
}

void ofApp::keyPressed(int key) {
	if(key == ' ') {
        debugMode = !debugMode;
    }
    if(key == 'f'){
        ofToggleFullscreen();
    }
    
    if(key == OF_KEY_BACKSPACE){
        if(setCorner != -1){
            cameraCalibrationCorners[setCorner] = ofVec2f(settings.getValue("corner"+ofToString(setCorner)+"x",0),
                                                          settings.getValue("corner"+ofToString(setCorner)+"y",0));
            
            updateCameraCalibration();
        }
        setCorner = -1;
    }
    
    if(debugMode){
        if(key == '1'){
            setCorner = 0;
        }
        if(key == '2'){
            setCorner = 1;
        }
        if(key == '3'){
            setCorner = 2;
        }
        if(key == '4'){
            setCorner = 3;
        }
    }
}

void ofApp::mouseMoved(int x, int y){
    if(setCorner != -1 ){
        cameraCalibrationCorners[setCorner] = ofVec2f(x / PREVIEW_SCALE, (y-300) / PREVIEW_SCALE);
        updateCameraCalibration();
        
    }
}

void ofApp::mousePressed( int x, int y, int button ){
    if(setCorner != -1){
        settings.setValue("corner"+ofToString(setCorner)+"x", int(cameraCalibrationCorners[setCorner].x));
        settings.setValue("corner"+ofToString(setCorner)+"y", int(cameraCalibrationCorners[setCorner].y));
        settings.save("settings.xml");
        
        setCorner = -1;
    }
}



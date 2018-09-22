/*
 This sends OSC to Max from every window, but if multiple windows are running
 with audio, then only one should be sending OSC. It would make sense to modify
 the server to send OSC, and the window(s) to update its brightness values
 */

#include "ofMain.h"
#include "ofAppNoWindow.h"
#include "ofAutoShader.h"
#include "ofAutoImage.h"
#include "ofxOsc.h"

const int n_stages = 7;
const int n_speakers = 4;
const int n_samples = 100;
#define USE_AUDIO

void renderScene(ofAutoShader& shader, ofFloatImage& xyz, ofFloatImage& confidence) {
    float mx = (float) ofGetMouseX() / ofGetWidth();
    float my = (float) ofGetMouseY() / ofGetHeight();
    shader.begin(); {
        shader.setUniformTexture("xyzMap", xyz, 0);
        shader.setUniformTexture("confidenceMap", confidence, 1);
        shader.setUniform1f("elapsedTime", ofGetElapsedTimef());
        shader.setUniform1i("frameNumber", ofGetFrameNum());
        shader.setUniform2f("mouse", ofVec2f(mx,my));
        xyz.draw(0, 0);
    } shader.end();
}

class ServerApp : public ofBaseApp {
public:
    bool debug = false;
    
    void setup() {
    }
    void update() {
    }
    void keyPressed(int key) {
        if(key == 'd') {
            debug = !debug;
        }
    }
    bool getDebug() {
        return debug;
    }
};

class ClientApp : public ofBaseApp {
public:
    int id;
    shared_ptr<ServerApp> server;
    
    ofAutoShader shader;
    ofAutoImage<float> xyzMap, confidenceMap;
    
#ifdef USE_AUDIO
    // audio output and speakers
    ofxOscSender oscSender;
    ofFloatImage speakerXyzMap;
    ofFloatImage speakerConfidenceMap;
    ofFbo speakerFbo;
    ofFloatPixels speakerPixels;
    float previousStage = -1;
#endif
    
    void config(int id, shared_ptr<ServerApp> server) {
        this->id = id;
        this->server = server;
        
        ofSetVerticalSync(true);
        ofDisableAntiAliasing();
        ofBackground(0);
        
        shader.loadAuto("../../../SharedData/shader/shader");
        
        xyzMap.loadAuto("../../../SharedData/xyzMap-" + ofToString(id) + ".exr");
        xyzMap.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        
        confidenceMap.loadAuto("../../../SharedData/confidenceMap-" + ofToString(id) + ".exr");
        confidenceMap.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        
        ofLog() << xyzMap.getWidth() << " x " << xyzMap.getHeight();
        ofLog() << confidenceMap.getWidth() << " x " << confidenceMap.getHeight();
        
#ifdef USE_AUDIO
        oscSender.setup("localhost", 7777);
        setupSpeakers();
#endif
    }
#ifdef USE_AUDIO
    void setupSpeakers() {
        ofVec3f speakers[n_speakers];
        // maybe need to swap dimensions here?
        // possibly change scale too
        float eps = 0.001; // needed to avoid "== 0" check in shader
        speakers[0] = ofVec3f(0,0,0)+eps; // front left
        speakers[1] = ofVec3f(0,1,0)+eps; // front right
        speakers[2] = ofVec3f(1,1,0)+eps; // rear right
        speakers[3] = ofVec3f(1,0,0)+eps; // rear left
        
        float speakerAreaSize = 0.02;
        speakerXyzMap.allocate(n_samples, n_speakers, OF_IMAGE_COLOR_ALPHA);
        speakerConfidenceMap.allocate(n_samples, n_speakers, OF_IMAGE_COLOR_ALPHA);
        
        float* xyzPixels = speakerXyzMap.getPixels().getData();
        float* confidencePixels = speakerConfidenceMap.getPixels().getData();
        for(int i = 0; i < n_speakers; i++){
            for(int j = 0; j < n_samples; j++){
                // sample a spiral
                float angle = j * TWO_PI / 20; // 20 samples per full rotation
                float radius = ((float) j / n_samples) * speakerAreaSize; // 0 to speakerAreaSize
                // might need to swap axes here too
                xyzPixels[0] = speakers[i].x + sin(angle) * radius;
                xyzPixels[1] = speakers[i].y + cos(angle) * radius;
                xyzPixels[2] = speakers[i].z;
                xyzPixels[3] = 1;
                xyzPixels += 4;
                
                confidencePixels[0] = 1;
                confidencePixels[1] = 1;
                confidencePixels[2] = 1;
                confidencePixels[3] = 1;
                confidencePixels += 4;
            }
        }
        speakerXyzMap.update();
        speakerConfidenceMap.update();
        
        speakerFbo.allocate(n_samples, n_speakers);
        speakerPixels.allocate(n_samples, n_speakers, OF_IMAGE_COLOR_ALPHA);
    }
#endif
    void update() {
#ifdef USE_AUDIO
        //Speaker sampling code
        speakerFbo.begin();
        renderScene(shader, speakerXyzMap, speakerConfidenceMap);
        speakerFbo.end();
        
        //Read back the fbo, and average it on the CPU
        speakerFbo.readToPixels(speakerPixels);
        speakerPixels.setImageType(OF_IMAGE_GRAYSCALE);
        
        ofxOscMessage brightnessMsg;
        brightnessMsg.setAddress("/audio/brightness");
        float* pix = speakerPixels.getData();
        for(int i = 0; i < n_speakers; i++){
            float avg = 0;
            for(int j = 0; j < n_samples; j++){
                avg += *pix++;
            }
            avg /= n_samples;
            brightnessMsg.addFloatArg(avg);
        }
        oscSender.sendMessage(brightnessMsg);
        
        float elapsedTime = ofGetElapsedTimef();
        // copied from shader --- 8< ---
        float t = elapsedTime / 30.; // duration of each stage
        float stage = floor(t); // index of current stage
        float i = t - stage; // progress in current stage
        // copied from shader --- 8< ---
        
        if(stage != previousStage) {
            ofxOscMessage msg;
            msg.setAddress("/audio/scene_change_event");
            msg.addIntArg(stage == 0 ? 0 : 2);
            oscSender.sendMessage(msg);
        }
        previousStage = stage;
        
        if(stage == 0) {
            float lighthouseAngle = ofGetElapsedTimef() / TWO_PI;
            lighthouseAngle += 0; // set offset here
            ofxOscMessage msg;
            msg.setAddress("/audio/lighthouse_angle");
            msg.addFloatArg(fmodf(lighthouseAngle, 1));
            oscSender.sendMessage(msg);
        }
        
#endif
    }
    void draw() {
        ofEnableAlphaBlending();
        ofSetColor(255);
        ofHideCursor();
        
        renderScene(shader, xyzMap, confidenceMap);
        
        if (server->getDebug()) {
            speakerXyzMap.draw(mouseX, mouseY);
            speakerFbo.draw(mouseX, mouseY+8);
            string msg = ofToString(id) + ": " + ofToString(round(ofGetFrameRate())) + "fps";
            ofDrawBitmapString(msg, 10, 20);
        }
    }
    void keyPressed(int key) {
        server->keyPressed(key);
        
        if(key > '0' && key < '9') {
            ofxOscMessage msg;
            msg.setAddress("/audio/scene_change_event");
            msg.addIntArg(key - '0');
            oscSender.sendMessage(msg);
        }
    }
};

int main() {
    ofSetLogLevel(OF_LOG_VERBOSE);
    ofJson config = ofLoadJson("../../../SharedData/settings.json");
    
    shared_ptr<ofAppNoWindow> winServer(new ofAppNoWindow);
    shared_ptr<ServerApp> appServer(new ServerApp);
    ofRunApp(winServer, appServer);
    
    ofGLFWWindowSettings settings;
    settings.setGLVersion(3,2);
    settings.decorated = false;
    settings.numSamples = 1;
    settings.resizable = true;
    
    int n = config["projectors"].size();
    for(int i = 0; i < n; i++) {
        ofJson curConfig = config["projectors"][i];
        settings.monitor = curConfig["monitor"];
        settings.setSize(curConfig["width"], curConfig["height"]);
        settings.setPosition(ofVec2f(curConfig["xwindow"], curConfig["ywindow"]));
        settings.multiMonitorFullScreen = curConfig["multimonitor"];
        settings.windowMode = curConfig["fullscreen"] ? OF_FULLSCREEN : OF_WINDOW;
        shared_ptr<ofAppBaseWindow> winClient = ofCreateWindow(settings);
        shared_ptr<ClientApp> appClient(new ClientApp);
        appClient->config(i, appServer);
        ofRunApp(winClient, appClient);
    }
    
    ofRunMainLoop();
}

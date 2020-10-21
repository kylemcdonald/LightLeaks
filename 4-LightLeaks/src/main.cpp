/*
 This sends OSC to Max from every window, but if multiple windows are running
 with audio, then only one should be sending OSC. It would make sense to modify
 the server to send OSC, and the window(s) to update its brightness values
 */

#include "ofMain.h"
#include "ofAppNoWindow.h"
#include "ofAutoShader.h"
#include "ofAutoImage.h"
#include "ofxWebServer.h"

#define USE_AUDIO
#ifdef USE_AUDIO
#include "ofxOsc.h"
#endif

#define USE_SYPHON
#ifdef USE_SYPHON
#include "ofxSyphon.h"
#endif

const int n_stages = 7;
const int n_speakers = 4;
const int n_samples = 100;

float elapsed_time_start = 0;
void startElapsedTimef() {
    elapsed_time_start = ofGetElapsedTimef();
}
float getElapsedTimef() {
    return ofGetElapsedTimef() - elapsed_time_start;
}

void renderScene(ofAutoShader& shader, ofFloatImage& xyz, ofFloatImage& confidence, ofFloatImage& mask, float fadeStatus, int debugMode, bool audio=false
#ifdef USE_SYPHON
                 ,ofxSyphonClient *syphonClient=nullptr
#endif
) {
    float mx = (float) ofGetMouseX() / ofGetWidth();
    float my = (float) ofGetMouseY() / ofGetHeight();
    shader.begin(); {
        shader.setUniformTexture("xyzMap", xyz, 0);
        shader.setUniformTexture("confidenceMap", confidence, 1);
        shader.setUniformTexture("mask", mask, 2);
#ifdef USE_SYPHON
        if(syphonClient != nullptr) {
            syphonClient->bind();
            shader.setUniformTexture("syphon", syphonClient->getTexture(), 3);
            shader.setUniform2f("syphonSize", syphonClient->getWidth(), syphonClient->getHeight());
            syphonClient->unbind();
        }
#endif
        shader.setUniform1f("elapsedTime", getElapsedTimef());
        shader.setUniform1i("frameNumber", ofGetFrameNum());
        shader.setUniform2f("mouse", ofVec2f(mx,my));
        shader.setUniform1i("audio", audio ? 1 : 0);
        shader.setUniform1f("fadeStatus", fadeStatus);
        shader.setUniform1i("debugMode", debugMode);
        if(audio) {
            // audio mode uses the xyz size
            xyz.draw(0, 0);
        } else {
            // non-audio uses the screen size
            xyz.draw(0, 0, ofGetWidth(), ofGetHeight());
        }
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

class ClientApp : public ofBaseApp, public ofxWSRequestHandler {
public:
    int id;
    shared_ptr<ServerApp> server;
    ofVec2f windowSize, windowPosition;
    vector<ofVec3f> speakerPositions;
    
    ofAutoShader shader, defaultShader;
    ofAutoImage<float> xyzMap, confidenceMap, mask;
    
    float fadeStatus = 1;
    int debugMode = -1;
    
    ofxWebServer webserver;
    
#ifdef USE_AUDIO
    // audio output and speakers
    ofxOscSender oscSender;
    ofFloatImage speakerXyzMap;
    ofFloatImage speakerConfidenceMap;
    ofFbo speakerFbo;
    ofFloatPixels speakerPixels;
    float previousStage = -1;
#endif
    
#ifdef USE_SYPHON
    ofxSyphonClient syphonClient;
#endif
    
    void httpGet(string url){
        if(ofIsStringInString(url,"/actions/start") == 1){
            ofLog() << "starting";
            fadeStatus = 1;
            httpResponse("Started");
            startElapsedTimef();
        }
        if(ofIsStringInString(url,"/actions/stop") == 1){
            ofLog() << "stopping";
            fadeStatus = 0;
            httpResponse("Stopped");
        }
        
        // handle debug modes
        debugMode = -1;
        if (ofIsStringInString(url, "/debug") == 1) {
            auto const parts = ofSplitString(url, "/");
            debugMode = ofToInt(parts[parts.size() - 1]);
        }
        
        // set confidence threshold
        
        
    }
    
    void config(int id, shared_ptr<ServerApp> server, ofVec2f windowSize, ofVec2f windowPosition, string serverName, string appName, vector<ofVec3f> speakerPositions) {
        this->id = id;
        this->server = server;
        this->windowSize = windowSize;
        this->windowPosition = windowPosition;
        this->speakerPositions = speakerPositions;
        
        ofSetVerticalSync(true);
        ofSetFrameRate(60); // due to https://github.com/openframeworks/openFrameworks/issues/6146
        ofDisableAntiAliasing();
        ofBackground(0);
        
        shader.loadAuto("../../../SharedData/shader/shader");
        defaultShader.loadAuto("../../../SharedData/shader/default");
        
        xyzMap.loadAuto("../../../SharedData/xyzMap-" + ofToString(id) + ".exr");
        xyzMap.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        
        confidenceMap.loadAuto("../../../SharedData/confidenceMap-" + ofToString(id) + ".exr");
        confidenceMap.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        
        mask.loadAuto("../../../SharedData/mask-" + ofToString(id) + ".png");
        mask.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        
        ofLog() << xyzMap.getWidth() << " x " << xyzMap.getHeight();
        ofLog() << confidenceMap.getWidth() << " x " << confidenceMap.getHeight();
        
        webserver.start("httpdocs", 8000);
        webserver.addHandler(this, "actions/*");
        webserver.addHandler(this, "debug/*");
        
#ifdef USE_AUDIO
        oscSender.setup("localhost", 7777);
        setupSpeakers();
#endif
        
#ifdef USE_SYPHON
        syphonClient.setup();
        syphonClient.set(serverName, appName);
#endif
    }
    ofAutoShader& getShader() {
        if(shader.isReady()) {
            return shader;
        }
        return defaultShader;
    }
#ifdef USE_AUDIO
    void setupSpeakers() {
        ofVec3f speakers[n_speakers];
        // maybe need to swap dimensions here?
        // possibly change scale too
        float eps = 0.001; // needed to avoid "== 0" check in shader
        for (int i = 0; i < 4; i++) {
            speakers[i] = speakerPositions[i] + eps;
        }
        
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
    }
#endif
    void update() {
        ofVec2f curSize(ofGetWindowWidth(), ofGetWindowHeight());
        ofVec2f curPosition(ofGetWindowPositionX(), ofGetWindowPositionY());
        if(curSize != windowSize || curPosition != windowPosition) {
            ofSetWindowShape(windowSize.x, windowSize.y);
            ofSetWindowPosition(windowPosition.x, windowPosition.y);
        }
        
#ifdef USE_AUDIO
        //Speaker sampling code
        speakerFbo.begin();
        renderScene(getShader(), speakerXyzMap, speakerConfidenceMap, mask, fadeStatus, -1, true);
        speakerFbo.end();
        
        // Read back the fbo, and average it on the CPU
        speakerFbo.readToPixels(speakerPixels);
        speakerPixels.setImageType(OF_IMAGE_GRAYSCALE);
        
        // Get the stage from the shader
        float* pix = speakerPixels.getData();
        int stage = speakerPixels[0] * 2;
        speakerPixels[0] = speakerPixels[1]; // overwrite with adjacent value
        
        ofxOscMessage brightnessMsg;
        brightnessMsg.setAddress("/audio/brightness");
        for(int i = 0; i < n_speakers; i++){
            float avg = 0;
            for(int j = 0; j < n_samples; j++){
                avg += *pix++;
            }
            avg /= n_samples;
            avg *= fadeStatus;
            brightnessMsg.addFloatArg(avg);
        }
        oscSender.sendMessage(brightnessMsg);
        
        if(stage != previousStage) {
            ofxOscMessage msg;
            msg.setAddress("/audio/scene_change_event");
            msg.addIntArg(stage);
            oscSender.sendMessage(msg);
        }
        previousStage = stage;
        
        if(stage == 0) {
            float lighthouseAngle = getElapsedTimef() / TWO_PI;
            lighthouseAngle += 0; // set offset here
            ofxOscMessage msg;
            msg.setAddress("/audio/lighthouse_angle");
            // line it up here because max won't save the preset
            msg.addFloatArg(fmodf(lighthouseAngle, 1));
            oscSender.sendMessage(msg);
        }
        
#endif
    }
    void draw() {
        ofEnableAlphaBlending();
        ofSetColor(255);
        ofHideCursor();
        
        renderScene(getShader(), xyzMap, confidenceMap, mask, fadeStatus, debugMode, false
#ifdef USE_SYPHON
                    , &syphonClient
#endif
                    );
        
        if (server->getDebug()) {
#ifdef USE_AUDIO
            speakerXyzMap.draw(mouseX, mouseY);
            speakerFbo.draw(mouseX, mouseY+8);
#endif
            string msg = ofToString(id) + ": " + ofToString(round(ofGetFrameRate())) + "fps";
            ofDrawBitmapString(msg, 10, 20);
        }
    }
    void keyPressed(int key) {
        server->keyPressed(key);
        
        if(key == 'r') {
            ofJson config = ofLoadJson("../../../SharedData/settings.json");
            string scanName = config["scanName"];
            string xyzMapFilename = "../../../SharedData/xyzMap-0" + scanName + ".exr";
            string confidenceMapFilename = "../../../SharedData/confidenceMap-0" + scanName + ".exr";
            xyzMap.setFilename(xyzMapFilename);
            xyzMap.reload(true);
            confidenceMap.setFilename(confidenceMapFilename);
            confidenceMap.reload(true);
        }
        
#ifdef USE_AUDIO
        if(key > '0' && key < '9') {
            ofxOscMessage msg;
            msg.setAddress("/audio/scene_change_event");
            msg.addIntArg(key - '0');
            oscSender.sendMessage(msg);
        }
#endif
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
        ofVec2f windowSize(curConfig["width"], curConfig["height"]);
        ofVec2f windowPosition(curConfig["xwindow"], curConfig["ywindow"]);
        string serverName = config["syphon"]["serverName"];
        string appName = config["syphon"]["appName"];
        vector<ofVec3f> speakerPositions;
        for (auto xyz : config["osc"]["speakers"]) {
            speakerPositions.push_back(ofVec3f(xyz[0], xyz[1], xyz[2]));
        }
        appClient->config(i, appServer, windowSize, windowPosition, serverName, appName, speakerPositions);
        ofRunApp(winClient, appClient);
    }
    
    ofRunMainLoop();
}

#include "ofMain.h"
#include "ofAppNoWindow.h"
#include "ofAutoShader.h"
#include "ofAutoImage.h"

ofJson jsonconfig;

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
    
    void config(int id, shared_ptr<ServerApp> server) {
        this->id = id;
        this->server = server;
        
        ofSetLogLevel(OF_LOG_VERBOSE);
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
    }
    
    void update() {
    }
    
    void draw() {
        ofEnableAlphaBlending();
        ofSetColor(255);
        ofHideCursor();
        
        shader.begin(); {
            shader.setUniformTexture("xyzMap", xyzMap, 0);
            shader.setUniformTexture("confidenceMap", confidenceMap, 1);
            shader.setUniform1f("elapsedTime", ofGetElapsedTimef());
            shader.setUniform2f("mouse", ofVec2f((float)mouseX/ofGetWidth(), (float)mouseY/ofGetHeight()));
            xyzMap.draw(0, 0, ofGetWidth(), ofGetHeight());
        } shader.end();
        
        if (server->getDebug()) {
            string msg = ofToString(id) + ": " + ofToString(round(ofGetFrameRate())) + "fps";
            ofDrawBitmapString(msg, 10, 20);
        }
    }
    
    void keyPressed(int key) {
        server->keyPressed(key);
    }
};

int main() {
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

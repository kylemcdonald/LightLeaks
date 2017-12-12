#include "ofMain.h"
#include "ofAppNoWindow.h"

class ServerApp : public ofBaseApp {
public:
    void setup() {
        ofLog() << "Running";
    }
    void update() {
    }
    int getAxis() {
        return int(ofGetElapsedTimef()*2) % 2;
    }
    int getLevel() {
        return int(ofGetElapsedTimef()) % 4;
    }
    int getInverted() {
        return int(ofGetElapsedTimef()*4) % 2;
    }
};

class ClientApp : public ofBaseApp {
public:
    int id, xcode, ycode;
    ofShader shader;
    shared_ptr<ServerApp> server;
    void config(int id, int xcode, int ycode, shared_ptr<ServerApp> server) {
        this->id = id;
        this->xcode = xcode;
        this->ycode = ycode;
        this->server = server;
    }
    
    void setup() {
        ofBackground(0);
        ofDisableAntiAliasing();
        shader.load("shader");
    }
    void update() {
    }
    void draw() {
        shader.begin();
        shader.setUniform1i("height", ofGetHeight());
        shader.setUniform1i("axis", server->getAxis());
        shader.setUniform1i("level", server->getLevel());
        shader.setUniform1i("inverted", server->getInverted());
        shader.setUniform1i("xcode", xcode);
        shader.setUniform1i("ycode", ycode);
        ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
        shader.end();
        
//        float r = ofMap(sin(ofGetElapsedTimef()), -1, 1, 0.3, 0.4) * ofGetHeight();
//        ofSetColor(128);
//        ofDrawCircle(ofGetWidth() / 2, ofGetHeight() / 2, r);
//        ofDrawBitmapStringHighlight(ofToString(id), 10, 20);
    }
    void keyPressed(int key) {
        cout << "ClientApp " << id << ": " << key << endl;
    }
};

ofVec2f getBoundingBox(const ofJson& projectors) {
    ofVec2f box;
    for(auto projector : projectors) {
        int x = int(projector["xcode"]) + int(projector["width"]);
        int y = int(projector["ycode"]) + int(projector["height"]);
        box.x = MAX(box.x, x);
        box.y = MAX(box.y, y);
    }
    return box;
}

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
        settings.width = curConfig["width"];
        settings.height = curConfig["height"];
        settings.setPosition(ofVec2f(curConfig["xwindow"], curConfig["ywindow"]));
        shared_ptr<ofAppBaseWindow> winClient = ofCreateWindow(settings);
        shared_ptr<ClientApp> appClient(new ClientApp);
        appClient->config(i, curConfig["xcode"], curConfig["ycode"], appServer);
        ofRunApp(winClient, appClient);
    }
    
    ofVec2f box = getBoundingBox(config["projectors"]);
    cout << "Bounding box: " << box << endl;
    
    ofRunMainLoop();
}

/*
 This app runs the projection during the calibration. It creates multiple windows
 and displays a coded image in each window. Each window has its own mask, which
 might not exist on the first run. The server app generates a list of patterns to
 be projected, and when it receives the "start" command it will iterate through them.
 */

#include "ofMain.h"
#include "ofAppNoWindow.h"
#include "ofAutoImage.h"
#include "ofAutoShader.h"
#include "ofxWebServer.h"

ofJson jsonconfig;

// this camera always returns after 500ms
class FakeCamera {
private:
    uint64_t lastTime = 0;
    bool newPhoto = false;
    bool waiting = false;
    bool startRequested = false;
    
public:
    void setup() {
        ofAddListener(ofEvents().update, this, &FakeCamera::updateInternal);
    }
    void updateInternal(ofEventArgs &args) {
        uint64_t curTime = ofGetElapsedTimeMillis();
        uint64_t timeDiff = 500;
        if (waiting) {
            if(curTime > lastTime + timeDiff) {
                newPhoto = true;
                waiting = false;
            }
        }
    }
    void takePhoto(string filename) {
        cout << filename << endl;
        lastTime = ofGetElapsedTimeMillis();
        waiting = true;
    }
    bool isStartRequested() {
        bool prevStartRequested = startRequested;
        startRequested = false;
        return prevStartRequested;
    }
    bool isPhotoNew() {
        bool prevNewPhoto = newPhoto;
        newPhoto = false;
        return prevNewPhoto;
    }
    
    // override the start request using this projection app
    void fakeStart() {
        startRequested = true;
    }
};


// this camera is connected on network
class NetworkCamera {
private:
    bool newPhoto = false;
    bool startRequested = false;
    
public:
    void setup() {
    }
    
    void takePhoto(string filename) {
        ofLog() <<"Take photo"<< filename;
        ofStringReplace(filename, "../../../SharedData", "/SharedData");
        string hostname = jsonconfig["osc"]["camera"];
        auto resp = ofLoadURL("http://"+hostname+":8080/actions/takePhoto/"+filename);
        if(resp.status != 200){
            ofLogError()<<"Could not trigger camera "<<resp.error<< " Status: "<<resp.status;
        } else {
            newPhoto = true;
        }
    }
    bool isStartRequested() {
        bool prevStartRequested = startRequested;
        startRequested = false;
        return prevStartRequested;
    }
    bool isPhotoNew() {
        bool prevNewPhoto = newPhoto;
        newPhoto = false;
        return prevNewPhoto;
    }
    
    // override the start request using this projection app
    void fakeStart() {
        startRequested = true;
    }
};

class ServerApp : public ofBaseApp, public ofxWSRequestHandler {
public:
    bool debug = false;
    bool capturing = false;
    bool needToCapture = false;
    int useColor = 0;
    ofVec3f color;
    uint64_t bufferTime = 100;
    uint64_t lastCaptureTime = 0;
    string timestamp;
    int pattern = 0;
    vector<tuple<int,int,int,int>> patterns;
    NetworkCamera camera;
    ofxWebServer webserver;
    
    void setup() {
        ofLog() << "Running";
        camera.setup();
        webserver.start("httpdocs", 8000);
        webserver.addHandler(this, "actions/*");
        
    }
    void config(ofVec2f box) {
        int xk = ceil(log2(box.x));
        int yk = ceil(log2(box.y));
        
        int axis, level, inverted, levelCount;
        axis = 0;
        for(level = 0; level < xk; level++) {
            for(int inverted : {0,1}) {
                patterns.emplace_back(make_tuple(axis, level, inverted, xk));
            }
        }
        axis = 1;
        for(level = 0; level < yk; level++) {
            for(int inverted : {0,1}) {
                patterns.emplace_back(make_tuple(axis, level, inverted, yk));
            }
        }
        
        cout << "Bounding box: " << box << endl;
        cout << "level count: " << xk << ", " << yk << endl;
        cout << "List of patterns:" << endl;
        for(auto cur : patterns) {
            tie(axis, level, inverted, levelCount) = cur;
            cout << "\t" << axis << ", " << level << ", " << inverted << ", " << levelCount << endl;
        }
    }
    bool nextState() {
        pattern++;
        if (pattern == patterns.size()) {
            pattern = 0;
            return false;
        }
        return true;
    }
    void update() {
        uint64_t curTime = ofGetElapsedTimeMillis();
        if(!capturing) {
            if(camera.isStartRequested()) {
                timestamp = ofToString(ofGetHours(),2,'0') + ofToString(ofGetMinutes(),2,'0');
                capturing = true;
                needToCapture = true;
            }
        }
        if(capturing && camera.isPhotoNew()) {
            if(nextState()) {
                lastCaptureTime = curTime;
                needToCapture = true;
            } else {
                cout << "Done taking photos. Hurray!" << endl;
                capturing = false;
            }
        }
        if(capturing) {
            if(needToCapture && curTime > bufferTime + lastCaptureTime) {
                string directory = "../../../SharedData/scan-" + timestamp + "/cameraImages/" +
                (getAxis() == 0 ? "vertical/" : "horizontal/") +
                (getInverted() == 0 ? "normal/" : "inverse/");
                // we need to invert here to keep with an older style
                string levelName = ofToString(getLevelCount() - getLevel() - 1);
                camera.takePhoto(directory + levelName + ".jpg");
                needToCapture = false;
            }
        }
    }
    void keyPressed(int key) {
        if(key == 'd') {
            debug = !debug;
        }
        if(key == 's') {
            camera.fakeStart();
        }
        if(key == OF_KEY_RIGHT) {
            pattern++;
        }
        if(key == OF_KEY_LEFT) {
            pattern--;
        }
        int n = patterns.size();
        pattern = (pattern + n) % n;
    }
    bool getDebug() {
        return debug;
    }
    int getAxis() {
        return get<0>(patterns[pattern]);
    }
    int getLevel() {
        return get<1>(patterns[pattern]);
    }
    int getInverted() {
        return get<2>(patterns[pattern]);
    }
    int getLevelCount() {
        return get<3>(patterns[pattern]);
    }
    
    void start(){
        pattern = 0;
        camera.fakeStart();
    }
    
    void httpGet(string url){
        if(ofIsStringInString(url,"/actions/start") == 1){
            start();
            httpResponse("Started");
        }
        if(ofIsStringInString(url,"/actions/stop") == 1){
            capturing = false;
            needToCapture = false;
            httpResponse("Stopped");
        }
        if(ofIsStringInString(url,"/actions/color") == 1){
            useColor = 1;
            
            string p = "" + url;
            ofStringReplace(p, "/actions/color/", "");
            vector<string> parts = ofSplitString(p, ",");
            color.set(ofToFloat(parts[0]), ofToFloat(parts[1]), ofToFloat(parts[2]));
            httpResponse("Set color to " + ofToString(color));
        }
        if(ofIsStringInString(url,"/actions/cameraHostname") == 1){
            string hostname = jsonconfig["osc"]["camera"];
            httpResponse(hostname);
        }
        if(ofIsStringInString(url,"/actions/currentPattern") == 1){
            httpResponse(ofToString(pattern)+"/"+ofToString(patterns.size()));
        }
        
        if(ofIsStringInString(url, "/actions/numPatterns") == 1) {
            httpResponse(ofToString(patterns.size()));
        }
        
        if(ofIsStringInString(url, "/actions/pattern") == 1) {
            useColor = 0;
            
            string p = ""+url;
            ofStringReplace(p, "/actions/pattern/", "");
            pattern = ofToInt(p);
            
            string outputName = (getAxis() == 0 ? "vertical/" : "horizontal/");
            outputName += (getInverted() == 0 ? "normal/" : "inverse/");
            // we need to invert here to keep with an older style
            string levelName = ofToString(getLevelCount() - getLevel() - 1);
            outputName += levelName;
            httpResponse(outputName);
        }
        
        
    }
};

class ClientApp : public ofBaseApp {
public:
    string codeType;
    int id, xcode, ycode;
    float hue;
    shared_ptr<ServerApp> server;
    
    ofAutoShader shader;
    ofAutoImage<unsigned char> mask;
    ofImage xcodeImage, ycodeImage;
    
    void config(int id, int n, string codeType, int xcode, int ycode, shared_ptr<ServerApp> server) {
        this->id = id;
        this->hue = id / float(n);
        this->codeType = codeType;
        this->xcode = xcode;
        this->ycode = ycode;
        this->server = server;
        mask.loadAuto("../../../SharedData/mask-" + ofToString(id) + ".png");
        
        int width = ofGetWidth();
        int height = ofGetHeight();
        int xbits = ceilf(log2(width));
        int ybits = ceilf(log2(height));
        string codeDir = "../../../codes/" + codeType + "/";
        xcodeImage.load(codeDir + ofToString(xbits) + ".png");
        ycodeImage.load(codeDir + ofToString(ybits) + ".png");
    }
    void setup() {
        ofLog() << "Running setup()";
        ofBackground(0);
        ofSetVerticalSync(true);
        ofHideCursor();
        ofDisableAntiAliasing();
        shader.loadAuto("shader");
    }
    void update() {
    }
    void draw() {
        ofImage& curCodeImage = server->getAxis() == 0 ? xcodeImage : ycodeImage;
        curCodeImage.getTexture().setTextureMinMagFilter(GL_NEAREST,GL_NEAREST);
        
        shader.begin();
        shader.setUniform1i("useColor", server->useColor);
        shader.setUniform3f("color", server->color);
        shader.setUniform1i("height", ofGetHeight());
        shader.setUniform1i("axis", server->getAxis());
        shader.setUniform1i("level", server->getLevel());
        shader.setUniform1i("inverted", server->getInverted());
        shader.setUniform1i("xcode", xcode);
        shader.setUniform1i("ycode", ycode);
        shader.setUniformTexture("code", curCodeImage, 0);
        ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
        shader.end();
        
        ofPushStyle();
        if(server->getDebug()) {
            ofSetColor(ofColor_<float>::fromHsb(hue, 1, 1));
            ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
            string fps = ofToString(int(round(ofGetFrameRate())));
            ofDrawBitmapStringHighlight(ofToString(id) + "/" + fps, 10, 20);
        } else {
            ofEnableBlendMode(OF_BLENDMODE_MULTIPLY);
            if(mask.isAllocated()) {
                mask.draw(0, 0);
            }
        }
        ofPopStyle();
    }
    void keyPressed(int key) {
        server->keyPressed(key);
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
    jsonconfig = ofLoadJson("../../../SharedData/settings.json");
    
    shared_ptr<ofAppNoWindow> winServer(new ofAppNoWindow);
    shared_ptr<ServerApp> appServer(new ServerApp);
    
    ofVec2f box = getBoundingBox(jsonconfig["projectors"]);
    appServer->config(box);
    ofRunApp(winServer, appServer);
    
    ofGLFWWindowSettings settings;
    settings.setGLVersion(3,2);
    settings.decorated = false;
    settings.numSamples = 1;
    settings.resizable = true;
    
    int n = jsonconfig["projectors"].size();
    for(int i = 0; i < n; i++) {
        ofJson curConfig = jsonconfig["projectors"][i];
        settings.monitor = curConfig["monitor"];
        settings.setSize(curConfig["width"], curConfig["height"]);
        settings.setPosition(ofVec2f(curConfig["xwindow"], curConfig["ywindow"]));
        settings.multiMonitorFullScreen = curConfig["multimonitor"];
        settings.windowMode = curConfig["fullscreen"] ? OF_FULLSCREEN : OF_WINDOW;
        shared_ptr<ofAppBaseWindow> winClient = ofCreateWindow(settings);
        shared_ptr<ClientApp> appClient(new ClientApp);
        appClient->config(i, n, curConfig["codeType"], curConfig["xcode"], curConfig["ycode"], appServer);
        ofLog() << "Launching window";
        ofRunApp(winClient, appClient);
    }
    
    ofRunMainLoop();
}

#include "ofApp.h"

int main() {
    shared_ptr<ofApp> mainApp(new ofApp);
    
    ofGLFWWindowSettings settings;
    settings.setGLVersion(3,2);
    settings.decorated = false;
    settings.numSamples = 1;
    settings.resizable = true;
    settings.setSize(5760, 1080);
    settings.setPosition(ofVec2f(-5760, 0));
    
    shared_ptr<ofAppBaseWindow> win = ofCreateWindow(settings);
    win->setVerticalSync(false);
    
    ofRunApp(win, mainApp);
    ofRunMainLoop();
}

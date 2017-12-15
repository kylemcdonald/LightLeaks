#include "ofApp.h"

int main() {
    shared_ptr<ofApp> mainApp(new ofApp);
    
    ofGLFWWindowSettings settings;
    settings.setGLVersion(3,2);
    settings.decorated = false;
    settings.numSamples = 1;
    settings.resizable = true;
    settings.width = 3840;
    settings.height = 2160;
    
    shared_ptr<ofAppBaseWindow> win = ofCreateWindow(settings);
    win->setVerticalSync(false);
    
    ofRunApp(win, mainApp);
    ofRunMainLoop();
}

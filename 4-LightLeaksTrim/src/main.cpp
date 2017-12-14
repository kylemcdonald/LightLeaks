#include "ofApp.h"

int main() {
    shared_ptr<ofApp> mainApp(new ofApp);
    
    ofGLFWWindowSettings settings;
    settings.width = 3840;
    settings.height = 2160;
    settings.resizable = false;
    settings.decorated = false;
    
    shared_ptr<ofAppBaseWindow> win = ofCreateWindow(settings);
    win->setVerticalSync(false);
    
    ofRunApp(win, mainApp);
    ofRunMainLoop();
}

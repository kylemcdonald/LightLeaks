#include "ofMain.h"
#include "ofApp.h"

int main() {
    ofGLFWWindowSettings windowSettings;
    windowSettings.setGLVersion(3, 2);
    windowSettings.setSize(1500, 1000);
    windowSettings.windowMode = OF_WINDOW;
    ofCreateWindow(windowSettings);
    ofRunApp(new ofApp());
}

#include "ofApp.h"
#include "ofAppGLFWWindow.h"


int main() {
    
    int numWindows = 3;

    shared_ptr<ofApp> mainApp(new ofApp);

    vector< shared_ptr<ofAppBaseWindow> > windows;
    /*for(int i=0;i<numWindows;i++){
        
        ofGLFWWindowSettings settings;
        settings.width = 600;
        settings.height = 600;
        settings.setPosition(ofVec2f(1920*i+100,0));
        settings.resizable = true;
        if(i > 0){
            settings.shareContextWith = ofGetMainLoop()->getCurrentWindow();
        }

        shared_ptr<ofAppBaseWindow> win = ofCreateWindow(settings);
        
        win->setVerticalSync(false);
        win->setFullscreen(true);
        windows.push_back(win);
        //win.setMultiDisplayFullscreen(true); //this makes the fullscreen window span across all your monitors
        
        ofRunApp(windows[i], mainApp);

    }*/
    
    ofGLFWWindowSettings settings;
    settings.width = 600;
    settings.height = 600;
    settings.setPosition(ofVec2f(1600,0));
    settings.resizable = false;
    settings.decorated = false;
    settings.monitor = 1;
    
    shared_ptr<ofAppBaseWindow> win = ofCreateWindow(settings);
    
    win->setVerticalSync(false);
    win->setWindowTitle("Projector 1");
    windows.push_back(win);
    

    
    settings.width = 600;
    settings.height = 600;
    settings.setPosition(ofVec2f(1920,0));
    settings.resizable = false;
    settings.shareContextWith = ofGetMainLoop()->getCurrentWindow();
    
    shared_ptr<ofAppBaseWindow> win2 = ofCreateWindow(settings);
    win2->setWindowTitle("Projector 2+3");
    win2->setVerticalSync(false);
    windows.push_back(win2);

  /*
    win->setWindowPosition(1920, 0);
    win->setWindowShape(1920,  1200);
    
    win2->setWindowPosition(1920, 0);

    */
    mainApp->windows = windows;
    
    ofRunApp(windows[0], mainApp);
    ofRunApp(windows[1], mainApp);

    ofRunMainLoop();
}

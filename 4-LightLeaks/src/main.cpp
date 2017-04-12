#include "ofApp.h"

int main() {
    
    shared_ptr<ofApp> mainApp(new ofApp);

    vector< shared_ptr<ofAppBaseWindow> > windows;
    
//    int numWindows = 1;
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
    settings.width = 4*1920;
    settings.height = 1080;
    settings.setPosition(ofVec2f(-1920*4,0)); // uncomment this!
    settings.resizable = false;
    settings.decorated = false;
//    settings.monitor = 0;
    
    shared_ptr<ofAppBaseWindow> win = ofCreateWindow(settings);
    win->setVerticalSync(false);
    win->setWindowTitle("Projector 1");
    windows.push_back(win);
//
//
//    
//    settings.width = 600;
//    settings.height = 600;
//    settings.setPosition(ofVec2f(1920,0));
//    settings.resizable = false;
//    settings.shareContextWith = ofGetMainLoop()->getCurrentWindow();
//    
//    shared_ptr<ofAppBaseWindow> win2 = ofCreateWindow(settings);
//    win2->setWindowTitle("Projector 2");
//    win2->setVerticalSync(false);
//    windows.push_back(win2);
//    
//    
//    settings.width = 600;
//    settings.height = 600;
//    settings.setPosition(ofVec2f(1920*2,0));
//    settings.resizable = false;
//    settings.shareContextWith = ofGetMainLoop()->getCurrentWindow();
//    
//    shared_ptr<ofAppBaseWindow> win3 = ofCreateWindow(settings);
//    win3->setWindowTitle("Projector 3");
//    win3->setVerticalSync(false);
//    windows.push_back(win3);
//    
//    settings.width = 600;
//    settings.height = 600;
//    settings.setPosition(ofVec2f(1920*3,0));
//    settings.resizable = false;
//    settings.shareContextWith = ofGetMainLoop()->getCurrentWindow();
//    
//    shared_ptr<ofAppBaseWindow> win4 = ofCreateWindow(settings);
//    win4->setWindowTitle("Projector 4");
//    win4->setVerticalSync(false);
//    windows.push_back(win4);
    
  /*
    win->setWindowPosition(1920, 0);
    win->setWindowShape(1920,  1200);
    
    win2->setWindowPosition(1920, 0);

    */
    mainApp->windows = windows;
//
    ofRunApp(windows[0], mainApp);
//    ofRunApp(windows[1], mainApp);
//    ofRunApp(windows[2], mainApp);
//    ofRunApp(windows[3], mainApp);
//
    ofRunMainLoop();
    
//    ofGLFWWindowSettings win;
//    
//    win.width = 1920*4;
//    win.height = 1080;
//    win.multiMonitorFullScreen = true;
//    
//    ofCreateWindow(win)->setFullscreen(true);
//    ofRunApp(new ofApp());
}

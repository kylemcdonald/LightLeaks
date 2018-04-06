#pragma once

#include "ofMain.h"
#include "ofxCv.h"
#include "ofxAssimpModelLoader.h"
#include "../../SharedCode/ofxProCamToolkit.h"
#include "ofxAutoControlPanel.h"
#include "../../SharedCode/LineArt.h"
#include "ofxGrabCam.h"

class ofApp : public ofBaseApp {
public:

	
	void setup();

	void setupControlPanel();
	void setupMesh();
	bool setupReference(string path);
	void setupFolders();
	void setupShader();



	void update();
	void updateRenderMode();

	void draw();
	void drawLabeledPoint(int label, ofVec2f position, ofColor color, ofColor bg = ofColor::black, ofColor fg = ofColor::white);

	void drawOverlay();
	void drawCalibration(ofColor pink);
	void drawReferenceImage();
	void drawSetupMode();
	void drawSelectionMode();
	void drawCamera();

	void render(ofColor color);


	void keyPressed(int key);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	

	void setb(string name, bool value);
	void seti(string name, int value);
	void setf(string name, float value);
	bool getb(string name);
	int geti(string name);
	float getf(string name);
	void saveCalibration();
	void saveXyzMap();

	void clearCalibration();

	ofDirectory workingDir;
	int currentIndex;
	string currentWorkingDir;
	vector<string> directoryList;

	bool bDisableCamera;
	ofxAssimpModelLoader loader;
	
	ofImage referenceImage;

	ofShader xyzShader, normalShader;
	float range;
	ofVec3f zero;
	ofFbo fboPositions, fboNormals;
		
	ofxGrabCam cam;
	ofVboMesh objectMesh;
	ofMesh imageMesh;
	ofLight light;
	ofxAutoControlPanel panel;
	
	vector<cv::Point3f> objectPoints;
	vector<cv::Point2f> imagePoints;
	vector<bool> referencePoints;
	
	cv::Mat rvec, tvec;
	ofMatrix4x4 modelMatrix;
	ofxCv::Intrinsics intrinsics;
	bool calibrationReady;
	
	time_t lastFragTimestamp, lastVertTimestamp;
	time_t lastFragTimestampXyz, lastVertTimestampXyz;
	ofShader shader;

	bool bRenderRGB;
	bool bRenderNormal;
	bool bDrawReferenceImage;

	ofColor offWhite;
};

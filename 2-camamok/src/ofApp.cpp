#include "ofApp.h"
#include "../../SharedCode/MeshUtils.h"
using namespace ofxCv;
using namespace cv;

void ofApp::setb(string name, bool value) {
	panel.setValueB(name, value);
}
void ofApp::seti(string name, int value) {
	panel.setValueI(name, value);
}
void ofApp::setf(string name, float value) {
	panel.setValueF(name, value);
}
bool ofApp::getb(string name) {
	return panel.getValueB(name);
}
int ofApp::geti(string name) {
	return panel.getValueI(name);
}
float ofApp::getf(string name) {
	return panel.getValueF(name);
}

void ofApp::setup() {

	ofSetVerticalSync(true);
	ofSetFrameRate(60);
	

	ofSetDrawBitmapMode(OF_BITMAPMODE_MODEL_BILLBOARD);
	ofSetLogLevel(OF_LOG_VERBOSE);
	calibrationReady = false;
	setupMesh();
	setupControlPanel();
	
	// LOAD SHADER FUNCTION
	setupShader();
	// Setup Reference Image
	setupFolders();
	setupReference(directoryList[currentIndex]);

	cam.setNearClip(1);
	cam.setFarClip(10000);
}

void ofApp::setupFolders() {
	workingDir.open("../../../SharedData/");
	workingDir.sort();
	
	for (int i = 0; i < workingDir.size(); i++) {
		string name = workingDir[i].getFileName();
		if (workingDir[i].isDirectory() && ofIsStringInString(name, "scan-") && !ofIsStringInString(name, "_")) {
			directoryList.push_back(workingDir.getPath(i));
		}
	}
	if (directoryList.size() > 0) {
		currentIndex = 0;
	}
	else {
		ofExit();
	}
}

void ofApp::setupShader() {
	xyzShader.load("../../../SharedData/shader/xyz.vs", "../../../SharedData/shader/xyz.fs");
	normalShader.load("../../../SharedData/shader/normal.vs", "../../../SharedData/shader/normal.fs");
}


void ofApp::update() {
	if (getb("selectionMode")) {

	}
	else {
		updateRenderMode();
	}
}

void ofApp::draw() {
	ofBackground(geti("backgroundColor"));
	if (getb("drawReferenceImage")) {
		drawReferenceImage();
	}

	if (getb("selectionMode")) {
		if (calibrationReady && getb("selectionModeWithCalibration")) {
			drawCalibration(ofColor(255, 255, 255, 75));
		}
		else {
			drawCamera();
		}
		drawSelectionMode();
	}
	else {
		if (calibrationReady) {
			if (getb("drawMesh") && getb("selectionModeWithCalibration")) {
				drawCalibration(ofColor(255, 255, 255, 75));
			}
			else {
				drawCalibration(ofColor(255, 255, 255, 100));
			}
		}
		drawOverlay();
		drawSetupMode();
	}


	if (getb("saveCalibration")) {
		saveCalibration();
		setb("saveCalibration", false);
	}
}

void ofApp::keyPressed(int key) {
	if (key == OF_KEY_BACKSPACE) { // delete selected
		if (getb("selected")) {
			setb("selected", false);
			int choice = geti("selectionChoice");
			referencePoints[choice] = false;
			imagePoints[choice] = Point2f();
		}
	}
	if (key == 'r') {
		setb("drawReferenceImage", !getb("drawReferenceImage"));
	}
	if (key == 'c') {
		setb("selectionModeWithCalibration", !getb("selectionModeWithCalibration"));
	}
	if (key == '\n') { // deselect
		setb("selected", false);
	}
	if (key == ' ') { // toggle render/select mode
		setb("selectionMode", !getb("selectionMode"));
		if (getb("selectionMode")) {
			cam.setMouseActionsEnabled(true);
		}
		else {
			cam.setMouseActionsEnabled(false);
		}
	}
	if (key == 's') {
		setb("saveCalibration", true);
	}
	if (key == '=') {
		currentIndex++;
		if (currentIndex >= directoryList.size()) {
			currentIndex = 0;
		}
		setupReference(directoryList[currentIndex]);
		clearCalibration();
	}
	if (key == '-') {
		currentIndex--;
		if (currentIndex < 0) {
			currentIndex = directoryList.size()-1;
		}
		setupReference(directoryList[currentIndex]);
		clearCalibration();
	}
}

void ofApp::mousePressed(int x, int y, int button) {
	if (x < panel.getPosX() + panel.getWidth() && x > panel.getPosX() &&
		y < panel.getPosY() + panel.getHeight() && y > panel.getPosY()) {
		bDisableCamera = true;
		cam.setMouseActionsEnabled(!bDisableCamera);
	}
	setb("selected", getb("hoverSelected"));
	seti("selectionChoice", geti("hoverChoice"));
	if (getb("selected")) {
		setb("dragging", true);
	}
}

void ofApp::mouseReleased(int x, int y, int button) {
	if (bDisableCamera) {
		bDisableCamera = false;
		cam.setMouseActionsEnabled(!bDisableCamera);
	}
	setb("dragging", false);
}

void ofApp::clearCalibration() {
	objectPoints.clear();
	imagePoints.clear();
	referencePoints.clear();

	int n = objectMesh.getNumVertices();
	objectPoints.resize(n);
	imagePoints.resize(n);
	referencePoints.resize(n, false);
	for (int i = 0; i < n; i++) {
		objectPoints[i] = toCv(objectMesh.getVertex(i));
	}
}

void ofApp::setupMesh() {
	if (loader.loadModel("../../../SharedData/model.dae", false)) { // must be false to avoid crashing
		objectMesh = collapseModel(loader);
		int n = objectMesh.getNumVertices();
		objectPoints.resize(n);
		imagePoints.resize(n);
		referencePoints.resize(n, false);
		for (int i = 0; i < n; i++) {
			objectPoints[i] = toCv(objectMesh.getVertex(i));
		}

		auto center = objectMesh.getCentroid();
	

		ofVec3f min, max;
		getBoundingBox(objectMesh, min, max);
		zero = min;
		ofVec3f diagonal = max - min;
		range = MAX(MAX(diagonal.x, diagonal.y), diagonal.z);
		cout << "Using min " << min << " max " << max << " and range " << range << endl;

		// move camera outside bounding box
		cam.setFixUpDirectionEnabled(true);

		cam.setPosition(max + ofVec3f(100, 0, 100));
		cam.lookAt(center, ofVec3f(0, 0, 1));
	}
	else {
	
	}
}

void ofApp::render(ofColor color) {
	ofPushStyle();
	ofSetLineWidth(geti("lineWidth"));
	if (getb("useSmoothing")) {
		ofEnableSmoothing();
	}
	else {
		ofDisableSmoothing();
	}


	ofSetColor(255);
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glEnable(GL_DEPTH_TEST);

	ofColor transparentBlack(0, 0, 0, 0);
	switch (geti("drawMode")) {
	case 0: // faces
		if (bRenderRGB) xyzShader.begin();
		//            glEnable(GL_CULL_FACE);
		//            glCullFace(GL_BACK);
		//            glDisable(GL_CULL_FACE);
		objectMesh.drawFaces();
		if (bRenderRGB) xyzShader.end();
		break;
	case 1: // fullWireframe
		ofSetColor(color);
		objectMesh.drawWireframe();
		break;
	case 2: // outlineWireframe
		ofSetColor(color);
		LineArt::draw(objectMesh, true, transparentBlack, NULL);
		break;
	case 3: // occludedWireframe
		ofSetColor(color);
		LineArt::draw(objectMesh, false, transparentBlack, NULL);
		break;
	}
	glPopAttrib();
	ofPopStyle();
}

void ofApp::saveCalibration() {
	if (calibrationReady) {
		string dirName = directoryList[currentIndex];
		ofDirectory dir(dirName);
		dir.create();

		FileStorage fs(dirName + "/calibration-advanced.yml", FileStorage::WRITE);

		Mat cameraMatrix = intrinsics.getCameraMatrix();
		fs << "cameraMatrix" << cameraMatrix;

		double focalLength = intrinsics.getFocalLength();
		fs << "focalLength" << focalLength;

		Point2d fov = intrinsics.getFov();
		fs << "fov" << fov;

		Point2d principalPoint = intrinsics.getPrincipalPoint();
		fs << "principalPoint" << principalPoint;

		cv::Size imageSize = intrinsics.getImageSize();
		fs << "imageSize" << imageSize;

		fs << "translationVector" << tvec;
		fs << "rotationVector" << rvec;

		Mat rotationMatrix;
		Rodrigues(rvec, rotationMatrix);
		fs << "rotationMatrix" << rotationMatrix;

		double rotationAngleRadians = norm(rvec, NORM_L2);
		double rotationAngleDegrees = ofRadToDeg(rotationAngleRadians);
		Mat rotationAxis = rvec / rotationAngleRadians;
		fs << "rotationAngleRadians" << rotationAngleRadians;
		fs << "rotationAngleDegrees" << rotationAngleDegrees;
		fs << "rotationAxis" << rotationAxis;

		ofVec3f axis(rotationAxis.at<double>(0), rotationAxis.at<double>(1), rotationAxis.at<double>(2));
		ofVec3f euler = ofQuaternion(rotationAngleDegrees, axis).getEuler();
		Mat eulerMat = (Mat_<double>(3, 1) << euler.x, euler.y, euler.z);
		fs << "euler" << eulerMat;

		ofFile basic("calibration-basic.txt", ofFile::WriteOnly);
		ofVec3f position(tvec.at<double>(1), tvec.at<double>(2));
		basic << "position (in world units):" << endl;
		basic << "\tx: " << ofToString(tvec.at<double>(0), 2) << endl;
		basic << "\ty: " << ofToString(tvec.at<double>(1), 2) << endl;
		basic << "\tz: " << ofToString(tvec.at<double>(2), 2) << endl;
		basic << "axis-angle rotation (in degrees):" << endl;
		basic << "\taxis x: " << ofToString(axis.x, 2) << endl;
		basic << "\taxis y: " << ofToString(axis.y, 2) << endl;
		basic << "\taxis z: " << ofToString(axis.z, 2) << endl;
		basic << "\tangle: " << ofToString(rotationAngleDegrees, 2) << endl;
		basic << "euler rotation (in degrees):" << endl;
		basic << "\tx: " << ofToString(euler.x, 2) << endl;
		basic << "\ty: " << ofToString(euler.y, 2) << endl;
		basic << "\tz: " << ofToString(euler.z, 2) << endl;
		basic << "fov (in degrees):" << endl;
		basic << "\thorizontal: " << ofToString(fov.x, 2) << endl;
		basic << "\tvertical: " << ofToString(fov.y, 2) << endl;
		basic << "principal point (in screen units):" << endl;
		basic << "\tx: " << ofToString(principalPoint.x, 2) << endl;
		basic << "\ty: " << ofToString(principalPoint.y, 2) << endl;
		basic << "image size (in pixels):" << endl;
		basic << "\tx: " << ofToString(principalPoint.x, 2) << endl;
		basic << "\ty: " << ofToString(principalPoint.y, 2) << endl;

		saveMat(Mat(objectPoints), dirName + "/objectPoints.yml");
		saveMat(Mat(imagePoints), dirName + "/imagePoints.yml");

		ofFloatPixels pix;
		fboPositions.readToPixels(pix);
		ofSaveImage(pix, dirName + "/xyzMap.exr");
		fboNormals.readToPixels(pix);
		ofSaveImage(pix, dirName + "/normalMap.exr");
	}
}

void ofApp::setupControlPanel() {
	panel.setup();
	panel.msg = "tab hides the panel, space toggles render/selection mode, 'f' toggles fullscreen.";

	panel.addPanel("Interaction");
	panel.addToggle("setupMode", true);
	panel.addSlider("backgroundColor", 0, 0, 255, true);
	panel.addMultiToggle("drawMode", 3, variadic("faces")("fullWireframe")("outlineWireframe")("occludedWireframe"));
	panel.addToggle("saveCalibration", false);
	panel.addToggle("drawXYZ", false);
	panel.addToggle("drawNormal", false);
	panel.addSlider("lineWidth", 2, 1, 8, true);
	panel.addToggle("drawMesh", true);
	panel.addToggle("useSmoothing", false);
	panel.addSlider("screenPointSize", 2, 1, 16, true);
	panel.addSlider("selectedPointSize", 8, 1, 16, true);
	panel.addSlider("selectionRadius", 12, 1, 32);
	panel.addToggle("drawReferenceImage", true);
	panel.addToggle("selectionModeWithCalibration", false);

	panel.addPanel("Internal");
	panel.addToggle("selectionMode", true);
	panel.addToggle("hoverSelected", false);
	panel.addSlider("hoverChoice", 0, 0, objectPoints.size(), true);
	panel.addToggle("selected", false);
	panel.addToggle("dragging", false);
	panel.addSlider("selectionChoice", 0, 0, objectPoints.size(), true);
	panel.addSlider("slowLerpRate", .001, 0, .01);
	panel.addSlider("fastLerpRate", 1, 0, 1);

	panel.addPanel("Calibration");
	panel.addSlider("aov", 80, 50, 100);
	panel.addToggle("CV_CALIB_FIX_ASPECT_RATIO", true);
	panel.addToggle("CV_CALIB_FIX_K1", true);
	panel.addToggle("CV_CALIB_FIX_K2", true);
	panel.addToggle("CV_CALIB_FIX_K3", true);
	panel.addToggle("CV_CALIB_ZERO_TANGENT_DIST", true);
	panel.addToggle("CV_CALIB_FIX_PRINCIPAL_POINT", false);

	offWhite = ofColor(255, 255, 255, 150);
}

bool ofApp::setupReference(string path) {
	bool referenceImageLoaded = referenceImage.load(path+"/reference-image.jpg");
	if (referenceImageLoaded) {

		referenceImage.resize(referenceImage.getWidth() / 4, referenceImage.getHeight() / 4);
		ofSetWindowShape(referenceImage.getWidth(), referenceImage.getHeight());

		ofFbo::Settings settings;
		settings.width = referenceImage.getWidth();
		settings.height = referenceImage.getHeight();
		settings.useDepth = true;
		settings.internalformat = GL_RGBA32F_ARB;
		fboPositions.allocate(settings);
		fboNormals.allocate(settings);
	}

	return referenceImageLoaded;
}

void ofApp::updateRenderMode() {
	// generate camera matrix given aov guess
	float aov = getf("aov");
	Size2i imageSize(referenceImage.getWidth(), referenceImage.getHeight());
	float f = imageSize.width * ofDegToRad(aov); // i think this is wrong, but it's optimized out anyway
	Point2f c = Point2f(imageSize) * (1. / 2);
	Mat1d cameraMatrix = (Mat1d(3, 3) <<
		f, 0, c.x,
		0, f, c.y,
		0, 0, 1);

	// generate flags
#define getFlag(flag) (panel.getValueB((#flag)) ? flag : 0)
	int flags =
		CV_CALIB_USE_INTRINSIC_GUESS |
		getFlag(CV_CALIB_FIX_PRINCIPAL_POINT) |
		getFlag(CV_CALIB_FIX_ASPECT_RATIO) |
		getFlag(CV_CALIB_FIX_K1) |
		getFlag(CV_CALIB_FIX_K2) |
		getFlag(CV_CALIB_FIX_K3) |
		getFlag(CV_CALIB_ZERO_TANGENT_DIST);

	vector<Mat> rvecs, tvecs;
	Mat distCoeffs;
	vector<vector<Point3f> > referenceObjectPoints(1);
	vector<vector<Point2f> > referenceImagePoints(1);
	int n = referencePoints.size();
	for (int i = 0; i < n; i++) {
		if (referencePoints[i]) {
			referenceObjectPoints[0].push_back(objectPoints[i]);
			referenceImagePoints[0].push_back(imagePoints[i]);
		}
	}
	const static int minPoints = 6;
	if (referenceObjectPoints[0].size() >= minPoints) {
		calibrateCamera(referenceObjectPoints, referenceImagePoints, imageSize, cameraMatrix, distCoeffs, rvecs, tvecs, flags);
		rvec = rvecs[0];
		tvec = tvecs[0];
		intrinsics.setup(cameraMatrix, imageSize);
		modelMatrix = makeMatrix(rvec, tvec);
		calibrationReady = true;
	}
	else {
		calibrationReady = false;
	}
}

void ofApp::drawLabeledPoint(int label, ofVec2f position, ofColor color, ofColor bg, ofColor fg) {
	ofPushStyle();
	glPushAttrib(GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	//glEnable(GL_DEPTH_TEST);
	ofVec2f tooltipOffset(5, -25);
	ofSetColor(color);
	float w = ofGetWindowWidth();
	float h = ofGetWindowHeight();
	ofSetLineWidth(0);
	ofNoFill();
	ofDrawLine(position - ofVec2f(w, 0), position + ofVec2f(w, 0));
	ofDrawLine(position - ofVec2f(0, h), position + ofVec2f(0, h));
	ofDrawCircle(position, geti("selectedPointSize"));
	ofDrawBitmapStringHighlight(ofToString(label), position + tooltipOffset, bg, fg);
	glPopAttrib();
	ofPopStyle();
}

void ofApp::drawCamera() {
	ofSetColor(255);
	cam.begin(ofGetCurrentViewport());
	render(ofColor(255, 255, 255, 100));
	imageMesh = getProjectedMesh(objectMesh);
	cam.end();
}

void ofApp::drawReferenceImage() {
	ofPushStyle();
	ofSetColor(255);
	referenceImage.draw(0, 0);
	ofPopStyle();
}

void ofApp::drawOverlay() {
	if (calibrationReady) {
		fboPositions.begin();
		ofClear(0);
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glPushMatrix();
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glMatrixMode(GL_MODELVIEW);


		intrinsics.loadProjectionMatrix(10, 20000000);
		applyMatrix(modelMatrix);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		xyzShader.begin();
		xyzShader.setUniform1f("range", range);
		xyzShader.setUniform3fv("zero", zero.getPtr());
		objectMesh.drawFaces();
		xyzShader.end();


		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopAttrib();
		fboPositions.end();

		fboNormals.begin();
		ofClear(0);
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glPushMatrix();
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glMatrixMode(GL_MODELVIEW);


		intrinsics.loadProjectionMatrix(10, 20000000);
		applyMatrix(modelMatrix);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		normalShader.begin();
		objectMesh.drawFaces();
		normalShader.end();


		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopAttrib();
		fboNormals.end();


		ofSetColor(255);
		ofPushMatrix();
		if (getb("drawXYZ")) fboPositions.draw(0, 0);
		if (getb("drawNormal")) fboNormals.draw(0, 0);
		ofPopMatrix();
	}
}

void ofApp::drawCalibration(ofColor color) {
	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);

	if (calibrationReady) {
		intrinsics.loadProjectionMatrix(10, 20000000);
		applyMatrix(modelMatrix);
		render(color);
		imageMesh = getProjectedMesh(objectMesh);
	}

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

void ofApp::drawSelectionMode() {
	if (getb("setupMode")) {

		// draw all points cyan small
		glPointSize(geti("screenPointSize"));
		glEnable(GL_POINT_SMOOTH);
		imageMesh.drawVertices();

		// draw all reference points cyan
		int n = referencePoints.size();
		for (int i = 0; i < n; i++) {
			if (referencePoints[i]) {
				drawLabeledPoint(i, imageMesh.getVertex(i), cyanPrint);
			}
		}

		// check to see if anything is selected
		// draw hover point magenta
		int choice;
		float distance;
		ofVec3f selected = getClosestPointOnMesh(imageMesh, mouseX, mouseY, &choice, &distance);
		if (!ofGetMousePressed() && distance < getf("selectionRadius")) {
			seti("hoverChoice", choice);
			setb("hoverSelected", true);
			drawLabeledPoint(choice, selected, magentaPrint);
		}
		else {
			setb("hoverSelected", false);
		}

		// draw selected point yellow
		if (getb("selected")) {
			int choice = geti("selectionChoice");
			ofVec2f selected = imageMesh.getVertex(choice);
			drawLabeledPoint(choice, selected, yellowPrint, ofColor::white, ofColor::black);
		}
	}
}

void ofApp::drawSetupMode() {
	if (getb("setupMode")) {
		// draw all reference points cyan
		int n = referencePoints.size();
		for (int i = 0; i < n; i++) {
			if (referencePoints[i]) {
				drawLabeledPoint(i, toOf(imagePoints[i]), cyanPrint);
				drawLabeledPoint(i, imageMesh.getVertex(i), offWhite, ofColor(0, 0, 0, 0), ofColor(0, 0, 0, 0));
			}
		}

		// move points that need to be dragged
		// draw selected yellow
		int choice = geti("selectionChoice");
		if (getb("selected")) {
			referencePoints[choice] = true;
			Point2f& cur = imagePoints[choice];
			if (cur == Point2f()) {
				cur = Point2f(mouseX, mouseY);
			}
			if (!getb("dragging")) {
				Point2f& cur = imagePoints[choice];
				cur.x += ofGetKeyPressed(OF_KEY_RIGHT) ? 0.01 : 0;
				cur.x -= ofGetKeyPressed(OF_KEY_LEFT) ? 0.01 : 0;
				cur.y -= ofGetKeyPressed(OF_KEY_UP) ? 0.01 : 0;
				cur.y += ofGetKeyPressed(OF_KEY_DOWN) ? 0.01 : 0;
				drawLabeledPoint(choice, toOf(cur), yellowPrint, ofColor::white, ofColor::black);
			}
		}
		if (getb("dragging")) {
			Point2f& cur = imagePoints[choice];
			float rate = ofGetMousePressed(0) ?getf("fastLerpRate") : getf("slowLerpRate");
			cur = Point2f(ofLerp(cur.x, mouseX, rate), ofLerp(cur.y, mouseY, rate));
			drawLabeledPoint(choice, toOf(cur), yellowPrint, ofColor::white, ofColor::black);
			ofSetColor(ofColor::black);
			ofDrawRectangle(toOf(cur), 1, 1);
		}
		else {
			// check to see if anything is selected
			// draw hover magenta
			float distance;
			ofVec2f selected = toOf(getClosestPoint(imagePoints, mouseX, mouseY, &choice, &distance));
			if (!ofGetMousePressed() && referencePoints[choice] && distance < getf("selectionRadius")) {
				seti("hoverChoice", choice);
				setb("hoverSelected", true);
				drawLabeledPoint(choice, selected, magentaPrint);
			}
			else {
				setb("hoverSelected", false);
			}
		}
	}
}




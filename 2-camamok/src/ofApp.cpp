#include "ofApp.h"

using namespace ofxCv;
using namespace cv;

void ofApp::setb(string name, bool value) {
    state[name] = value;
}
void ofApp::seti(string name, int value) {
    state[name] = value;
}
void ofApp::setf(string name, float value) {
    state[name] = value;
}
bool ofApp::getb(string name) {
    return state[name];
}
int ofApp::geti(string name) {
    return state[name];
}
float ofApp::getf(string name) {
	return state[name];
}

void getBoundingBox(const ofMesh& mesh, ofVec3f& min, ofVec3f& max) {
	int n = mesh.getNumVertices();
	if(n > 0) {
		min = mesh.getVertex(0);
		max = mesh.getVertex(0);
		for(int i = 1; i < n; i++) {
			const ofVec3f& cur = mesh.getVertices()[i];
			min.x = MIN(min.x, cur.x);
			min.y = MIN(min.y, cur.y);
			min.z = MIN(min.z, cur.z);
			max.x = MAX(max.x, cur.x);
			max.y = MAX(max.y, cur.y);
			max.z = MAX(max.z, cur.z);
		}
	}
}

void ofApp::setup() {
	ofSetDrawBitmapMode(OF_BITMAPMODE_MODEL_BILLBOARD);
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	calibrationReady = false;
	setupMesh();	
	setupState();
	ofSetLogLevel(OF_LOG_VERBOSE);
	
	xyzShader.load("xyz.vs", "xyz.fs");
	normalShader.load("normal.vs", "normal.fs");
	ofVec3f min, max;
	getBoundingBox(objectMesh, min, max);
	zero = min;
	ofVec3f diagonal = max - min;
	range = MAX(MAX(diagonal.x, diagonal.y), diagonal.z);
	cout << "Using min " << min << " max " << max << " and range " << range << endl;
	
	bool referenceImageLoaded = referenceImage.load("referenceImage.jpg");
    if(!referenceImageLoaded) throw "referenceImage.jpg is needed";
    
	referenceImage.resize(referenceImage.getWidth() / 4, referenceImage.getHeight() / 4);
	ofSetWindowShape(referenceImage.getWidth(), referenceImage.getHeight());
	
	ofFbo::Settings settings;
	settings.width = referenceImage.getWidth();
	settings.height = referenceImage.getHeight();
	settings.useDepth = true;
	settings.internalformat = GL_RGBA32F_ARB;
	fboPositions.allocate(settings);
	fboNormals.allocate(settings);
    
    cam.setFarClip(1000000);
    
}

void ofApp::update() {
	if(getb("randomLighting")) {
		setf("lightX", ofSignedNoise(ofGetElapsedTimef(), 1, 1) * 1000);
		setf("lightY", ofSignedNoise(1, ofGetElapsedTimef(), 1) * 1000);
		setf("lightZ", ofSignedNoise(1, 1, ofGetElapsedTimef()) * 1000);
	}
	light.setPosition(getf("lightX"), getf("lightY"), getf("lightZ"));
		
	if(getb("selectionMode")) {
	} else {
		updateRenderMode();
	}
}

void enableFog(float near, float far) {
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	GLfloat fogColor[4]= {0, 0, 0, 1};
	glFogfv(GL_FOG_COLOR, fogColor);
	glHint(GL_FOG_HINT, GL_FASTEST);
	glFogf(GL_FOG_START, near);
	glFogf(GL_FOG_END, far);
}

void disableFog() {
	glDisable(GL_FOG);
}

void ofApp::draw() {
	ofBackground(geti("backgroundColor"));
	if(getb("saveCalibration")) {
		saveCalibration();
		setb("saveCalibration", false);
	}
	if(getb("saveXyzMap")) {
		saveXyzMap();
		setb("saveXyzMap", false);
	}
	if(getb("selectionMode")) {
		drawSelectionMode();
	} else {
		drawOverlay();
		drawRenderMode();
	}
	if(!getb("validShader")) {
		ofPushStyle();
		ofSetColor(magentaPrint);
		ofSetLineWidth(8);
		ofDrawLine(0, 0, ofGetWidth(), ofGetHeight());
		ofDrawLine(ofGetWidth(), 0, 0, ofGetHeight());
		string message = "Shader failed to compile.";
		ofVec2f center(ofGetWidth(), ofGetHeight());
		center /= 2;
		center.x -= message.size() * 8 / 2;
		center.y -= 8;
		ofDrawBitmapStringHighlight(message, center);
		ofPopStyle();
	}
}

void ofApp::keyPressed(int key) {
	if(key == OF_KEY_BACKSPACE) { // delete selected
		if(getb("selected")) {
			setb("selected", false);
			int choice = geti("selectionChoice");
			referencePoints[choice] = false;
			imagePoints[choice] = Point2f();
		}
	}
	if(key == '\n') { // deselect
		setb("selected", false);
	}
	if(key == ' ') { // toggle render/select mode
		setb("selectionMode", !getb("selectionMode"));
	}
}

void ofApp::mousePressed(int x, int y, int button) {
	setb("selected", getb("hoverSelected"));
	seti("selectionChoice", geti("hoverChoice"));
	if(getb("selected")) {
		setb("dragging", true);
	}
}

void ofApp::mouseReleased(int x, int y, int button) {
	setb("dragging", false);
}

void ofApp::setupMesh() {
	model.loadModel("../../../SharedData/model.dae", false); // must be false to avoid crashing
    assert(model.getNumMeshes() == 1);
    objectMesh = model.getMesh(0);
//    ofVec3f offset(154, 39, 0);
//    for(auto& x : objectMesh.getVertices()) {
//        x -= offset;
//    }
	int n = objectMesh.getNumVertices();
	objectPoints.resize(n);
	imagePoints.resize(n);
	referencePoints.resize(n, false);
	for(int i = 0; i < n; i++) {
		objectPoints[i] = toCv(objectMesh.getVertex(i));
	}
    
    auto center = objectMesh.getCentroid();
//    cam.setTarget(center);
    cam.setPosition(center);
}

void ofApp::render() {
	ofPushStyle();
	ofSetLineWidth(geti("lineWidth"));
	if(getb("useSmoothing")) {
		ofEnableSmoothing();
	} else {
		ofDisableSmoothing();
	}
	int shading = geti("shading");
	bool useLights = shading == 1;
	bool useShader = shading == 2;
	if(useLights) {
		light.enable();
		ofEnableLighting();
		glShadeModel(GL_SMOOTH);
		glEnable(GL_NORMALIZE);
	}
	
	if(getb("highlight")) {
		objectMesh.clearColors();
		int n = objectMesh.getNumVertices();
		float highlightPosition = getf("highlightPosition");
		float highlightOffset = getf("highlightOffset");
		for(int i = 0; i < n; i++) {
			int lower = ofMap(highlightPosition - highlightOffset, 0, 1, 0, n);
			int upper = ofMap(highlightPosition + highlightOffset, 0, 1, 0, n);
			ofColor cur = (lower < i && i < upper) ? ofColor::white : ofColor::black;
			objectMesh.addColor(cur);
		}
	}
	
	ofSetColor(255);
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glEnable(GL_DEPTH_TEST);
	if(useShader) {
		ofFile fragFile("shader.frag"), vertFile("shader.vert");
        time_t fragTimestamp = filesystem::last_write_time(fragFile);
        time_t vertTimestamp = filesystem::last_write_time(vertFile);
		if(fragTimestamp != lastFragTimestamp || vertTimestamp != lastVertTimestamp) {
			bool validShader = shader.load("shader");
			setb("validShader", validShader);
		}
		lastFragTimestamp = fragTimestamp;
		lastVertTimestamp = vertTimestamp;
		
		shader.begin();
		shader.setUniform1f("elapsedTime", ofGetElapsedTimef());
		shader.end();
	}
	ofColor transparentBlack(0, 0, 0, 0);
	switch(geti("drawMode")) {
		case 0: // faces
			if(useShader) xyzShader.begin();
//            glEnable(GL_CULL_FACE);
//            glCullFace(GL_BACK);
//            glDisable(GL_CULL_FACE);
			objectMesh.drawFaces();
			if(useShader) xyzShader.end();
			break;
		case 1: // fullWireframe
			if(useShader) shader.begin();
			objectMesh.drawWireframe();
			if(useShader) shader.end();
			break;
		case 2: // outlineWireframe
			LineArt::draw(objectMesh, true, transparentBlack, useShader ? &shader : NULL);
			break;
		case 3: // occludedWireframe
			LineArt::draw(objectMesh, false, transparentBlack, useShader ? &shader : NULL);
			break;
	}
	glPopAttrib();
	if(useLights) {
		ofDisableLighting();
	}
	ofPopStyle();
}

void ofApp::saveCalibration() {
	string dirName = "calibration-" + ofGetTimestampString() + "/";
	ofDirectory dir(dirName);
	dir.create();
	
	FileStorage fs(ofToDataPath(dirName + "calibration-advanced.yml"), FileStorage::WRITE);	
	
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
	Mat eulerMat = (Mat_<double>(3,1) << euler.x, euler.y, euler.z);
	fs << "euler" << eulerMat;
	
	ofFile basic("calibration-basic.txt", ofFile::WriteOnly);
	ofVec3f position( tvec.at<double>(1), tvec.at<double>(2));
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
	
	saveMat(Mat(objectPoints), dirName + "objectPoints.yml");
	saveMat(Mat(imagePoints), dirName + "imagePoints.yml");
}

void ofApp::saveXyzMap() {
	ofFloatPixels pix;
	fboPositions.readToPixels(pix);
	ofSaveImage(pix, "xyzMap.exr");
	fboNormals.readToPixels(pix);
	ofSaveImage(pix, "normalMap.exr");
}

void ofApp::setupState() {
    // Interaction
    state["setupMode"] = true;
    state["scale"] = 1;
    state["backgroundColor"] = 0;
    state["drawMode"] = "occludedWireframe"; // faces, fullWireframe, outlineWireframe, occludedWireframe
    state["shading"] = "none";
    state["saveCalibration"] = false;
    state["saveXyzMap"] = false;
    
    // Highlight
    state["highlight"] = false;
    state["highlightPosition"] = 0;
    state["highlightOffset"] = .1;
    
    // Calibration
    state["aov"] = 80;
    state["CV_CALIB_FIX_ASPECT_RATIO"] = true;
    state["CV_CALIB_FIX_K1"] = true;
    state["CV_CALIB_FIX_K2"] = true;
    state["CV_CALIB_FIX_K3"] = true;
    state["CV_CALIB_ZERO_TANGENT_DIST"] = true;
    state["CV_CALIB_FIX_PRINCIPAL_POINT"] = false;
    
    // Rendering
    state["lineWidth"] = 2;
    state["useSmoothing"] = false;
    state["useFog"] = false;
    state["fogNear"] = 200;
    state["fogFar"] = 1850;
    state["screenPointSize"] = 2;
    state["selectedPointSize"] = 8;
    state["selectionRadius"] = 12;
    state["lightX"] = 200;
    state["lightY"] = 400;
    state["lightZ"] = 800;
    state["randomLighting"] = false;
    
    // Internal
    state["validShader"] = true;
    state["selectionMode"] = true;
    state["hoverSelected"] = false;
    state["hoverChoice"] = 0;
    state["selected"] = false;
    state["dragging"] = false;
    state["selectionChoice"] = 0;
    state["slowLerpRate"] = .001;
    state["fastLerpRate"] = 1;
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
	#define getFlag(flag) (getb((#flag)) ? flag : 0)
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
	for(int i = 0; i < n; i++) {
		if(referencePoints[i]) {
			referenceObjectPoints[0].push_back(objectPoints[i]);
			referenceImagePoints[0].push_back(imagePoints[i]);
		}
	}
	const static int minPoints = 6;
	if(referenceObjectPoints[0].size() >= minPoints) {
		calibrateCamera(referenceObjectPoints, referenceImagePoints, imageSize, cameraMatrix, distCoeffs, rvecs, tvecs, flags);
		rvec = rvecs[0];
		tvec = tvecs[0];
		intrinsics.setup(cameraMatrix, imageSize);
		modelMatrix = makeMatrix(rvec, tvec);
		calibrationReady = true;
	} else {
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
	float w = ofGetWidth();
	float h = ofGetHeight();
	ofSetLineWidth(0);
	ofNoFill();
	ofDrawLine(position - ofVec2f(w,0), position + ofVec2f(w,0));
	ofDrawLine(position - ofVec2f(0,h), position + ofVec2f(0,h));
	ofDrawCircle(position, geti("selectedPointSize"));
	ofDrawBitmapStringHighlight(ofToString(label), position + tooltipOffset, bg, fg);
	glPopAttrib();
	ofPopStyle();
}
	
void ofApp::drawSelectionMode() {
	ofSetColor(255);
	cam.begin();
	float scale = getf("scale");
	ofScale(scale, scale, scale);
	if(getb("useFog")) {
		enableFog(getf("fogNear"), getf("fogFar"));
	}
	render();
	if(getb("useFog")) {
		disableFog();
	}
	if(getb("setupMode")) {
		imageMesh = getProjectedMesh(objectMesh);
	}
	cam.end();
	
	if(getb("setupMode")) {
		// draw all points cyan small
		glPointSize(geti("screenPointSize"));
		glEnable(GL_POINT_SMOOTH);
		ofSetColor(cyanPrint);
		imageMesh.drawVertices();

		// draw all reference points cyan
		int n = referencePoints.size();
		for(int i = 0; i < n; i++) {
			if(referencePoints[i]) {
				drawLabeledPoint(i, imageMesh.getVertex(i), cyanPrint);
			}
		}
		
		// check to see if anything is selected
		// draw hover point magenta
		int choice;
		float distance;
		ofVec3f selected = getClosestPointOnMesh(imageMesh, mouseX, mouseY, &choice, &distance);
		if(!ofGetMousePressed() && distance < getf("selectionRadius")) {
			seti("hoverChoice", choice);
			setb("hoverSelected", true);
			drawLabeledPoint(choice, selected, magentaPrint);
		} else {
			setb("hoverSelected", false);
		}
		
		// draw selected point yellow
		if(getb("selected")) {
			int choice = geti("selectionChoice");
			ofVec2f selected = imageMesh.getVertex(choice);
			drawLabeledPoint(choice, selected, yellowPrint, ofColor::white, ofColor::black);
		}
	}
}

void ofApp::drawOverlay() {
	ofPushStyle();
	ofSetColor(255);
	referenceImage.draw(0, 0);
	ofPopStyle();
	
	if(ofGetKeyPressed('o')) {
		fboPositions.begin();
		ofClear(0);
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glPushMatrix();
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glMatrixMode(GL_MODELVIEW);
		
		if(calibrationReady) {
			intrinsics.loadProjectionMatrix(10, 20000000);
			applyMatrix(modelMatrix);
			glEnable(GL_DEPTH_TEST);
//            glEnable(GL_CULL_FACE);
//            glCullFace(GL_BACK);
            glDisable(GL_CULL_FACE);
			xyzShader.begin();
			xyzShader.setUniform1f("range", range);
			xyzShader.setUniform3fv("zero", zero.getPtr());
			objectMesh.drawFaces();
			xyzShader.end();
		}
		
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
		
		if(calibrationReady) {
			intrinsics.loadProjectionMatrix(10, 20000000);
			applyMatrix(modelMatrix);
			glEnable(GL_DEPTH_TEST);
//            glEnable(GL_CULL_FACE);
//            glCullFace(GL_BACK);
            glDisable(GL_CULL_FACE);
			normalShader.begin();
			objectMesh.drawFaces();
			normalShader.end();
		}
		
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopAttrib();
		fboNormals.end();
		
		ofSetColor(255);
		ofPushMatrix();
		fboPositions.draw(0, 0);
//		fboNormals.draw(fboPositions.getWidth() / 2, 0);
		ofPopMatrix();
	}
}

void ofApp::drawRenderMode() {	
	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	
	if(calibrationReady) {
		intrinsics.loadProjectionMatrix(10, 20000000);
		applyMatrix(modelMatrix);
		render();
		if(getb("setupMode")) {
			imageMesh = getProjectedMesh(objectMesh);	
		}
	}
	
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	
	if(getb("setupMode")) {
		// draw all reference points cyan
		int n = referencePoints.size();
		for(int i = 0; i < n; i++) {
			if(referencePoints[i]) {
				drawLabeledPoint(i, toOf(imagePoints[i]), cyanPrint);
			}
		}
		
		// move points that need to be dragged
		// draw selected yellow
		int choice = geti("selectionChoice");
		if(getb("selected")) {
			referencePoints[choice] = true;	
			Point2f& cur = imagePoints[choice];	
			if(cur == Point2f()) {
				if(calibrationReady) {
					cur = toCv(ofVec2f(imageMesh.getVertex(choice)));
				} else {
					cur = Point2f(mouseX, mouseY);
				}
			}
		}
		if(getb("dragging")) {
			Point2f& cur = imagePoints[choice];
			float rate = ofGetMousePressed(0) ? getf("slowLerpRate") : getf("fastLerpRate");
			cur = Point2f(ofLerp(cur.x, mouseX, rate), ofLerp(cur.y, mouseY, rate));
			drawLabeledPoint(choice, toOf(cur), yellowPrint, ofColor::white, ofColor::black);
			ofSetColor(ofColor::black);
			ofDrawRectangle(toOf(cur), 1, 1);
		} else {
			// check to see if anything is selected
			// draw hover magenta
			float distance;
			ofVec2f selected = toOf(getClosestPoint(imagePoints, mouseX, mouseY, &choice, &distance));
			if(!ofGetMousePressed() && referencePoints[choice] && distance < getf("selectionRadius")) {
				seti("hoverChoice", choice);
				setb("hoverSelected", true);
				drawLabeledPoint(choice, selected, magentaPrint);
			} else {
				setb("hoverSelected", false);
			}
		}
	}
}

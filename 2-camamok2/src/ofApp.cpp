#include "ofApp.h"

ofMesh collapseModel(ofxAssimpModelLoader& model) {
    ofMesh mesh;
    for (int i = 0; i < model.getNumMeshes(); i++) {
        mesh.append(ofMesh(model.getMesh(i)));
    }
    return mesh;
}

void scaleMesh(ofMesh& mesh, float scale) {
    for (auto& v : mesh.getVertices()) {
        v *= scale;
    }
}

void fixAxes(ofMesh& mesh) {
    for (auto& v : mesh.getVertices()) {
        std::swap(v.y, v.z);
        v.z *= -1;
    }
}

vector<ofVec3f> projectPoints(const ofCamera& camera, const ofMesh& mesh) {
    vector<ofVec3f> points;
    for (auto& v : mesh.getVertices()) {
        points.emplace_back(camera.worldToScreen(v));
    }
    return points;
}

// 3d version for mesh view
unsigned int pickPoints(const vector<ofVec3f>& points, const ofVec2f& cursor) {
    unsigned int closestIndex = 0;
    unsigned int i = 0;
    float closestDistance = 0;
    for (auto& v : points) {
        if (v.z < 1) { // cull
            float dx = cursor.x - v.x, dy = cursor.y - v.y;
            float distance = dx * dx + dy * dy;
            if (closestIndex == 0 || distance < closestDistance) {
                closestDistance = distance;
                closestIndex = i;
            }
        }
        i++;
    }
    return closestIndex;
}

// 2d version for image view
unsigned int  pickPoints(const map<unsigned int, ofVec2f>& points, const ofVec2f& cursor) {
    unsigned int closestIndex = 0;
    float closestDistance = 0;
    for (auto& pair : points) {
        auto& v = pair.second;
        float dx = cursor.x - v.x, dy = cursor.y - v.y;
        float distance = dx * dx + dy * dy;
        if (closestIndex == 0 || distance < closestDistance) {
            closestDistance = distance;
            closestIndex = pair.first;
        }
    }
    return closestIndex;
}

bool findCameraAndModelMatrix(const vector<ofVec3f>& objectPoints, const vector<ofVec2f>& imagePoints, ofVec2f imageSize, ofMatrix4x4& cameraMatrix, ofMatrix4x4& modelMatrix) {
    const static int minPoints = 6;
    if (objectPoints.size() < minPoints) return false;
    
    // generate camera matrix given aov guess
    float aov = 80;
    cv::Size2i cvImageSize(imageSize.x, imageSize.y);
    float f = cvImageSize.width * ofDegToRad(aov); // i think this is wrong, but it's optimized out anyway
    cv::Point2f c = cv::Point2f(cvImageSize) * (1. / 2);
    cv::Mat1d cvCameraMatrix = (cv::Mat1d(3, 3) <<
                                f, 0, c.x,
                                0, f, c.y,
                                0, 0, 1);
    
    // generate flags
    int flags =
    CV_CALIB_USE_INTRINSIC_GUESS |
    //    CV_CALIB_FIX_PRINCIPAL_POINT |
    CV_CALIB_FIX_ASPECT_RATIO |
    CV_CALIB_FIX_K1 |
    CV_CALIB_FIX_K2 |
    CV_CALIB_FIX_K3 |
    CV_CALIB_ZERO_TANGENT_DIST;
    
    cv::Mat distCoeffs;
    vector<vector<cv::Point3f>> cvObjectPoints(1);
    vector<vector<cv::Point2f>> cvImagePoints(1);
    int n = objectPoints.size();
    for (int i = 0; i < n; i++) {
        cvObjectPoints[0].emplace_back(objectPoints[i].x, objectPoints[i].y, objectPoints[i].z);
        cvImagePoints[0].emplace_back(imagePoints[i].x, imagePoints[i].y);
    }
    
    vector<cv::Mat> rvecs, tvecs;
    calibrateCamera(cvObjectPoints, cvImagePoints, cvImageSize, cvCameraMatrix, distCoeffs, rvecs, tvecs, flags);
    cv::Mat1f cvCameraMatrixf = cvCameraMatrix;
    cameraMatrix = ofMatrix4x4((float*) cvCameraMatrixf.ptr());
    modelMatrix = ofxCv::makeMatrix(rvecs[0], tvecs[0]);
    return true;
}

void ofApp::setup(){
    loader.loadModel("model.dae");
    image.load("referenceImage.jpg");
    mesh = collapseModel(loader);
    scaleMesh(mesh, 0.01); // cm to m
    fixAxes(mesh); // switch from sketchup to OF axes
    
    glPointSize(6);
    
    ofBackground(0);
    ofEnableSmoothing();
    
    camera.setFixUpDirectionEnabled(true);
    camera.setPosition(0,0,0);
    camera.lookAt(ofVec3f(1,0,0));
}

void ofApp::update(){
    updateCalibration();
}

void ofApp::updateCalibration() {
    vector<ofVec3f> objectPoints;
    vector<ofVec2f> imagePoints;
    for(auto& pair : mappedIndices) {
        objectPoints.push_back(mesh.getVertices()[pair.first]);
        imagePoints.push_back(pair.second);
    }
    // this should be the image size, and all the imagePoints should be scaled to the image size
    ofVec2f imageSize(ofGetWidth(), ofGetHeight());
    bool previousReady = calibrationReady;
    calibrationReady = findCameraAndModelMatrix(objectPoints, imagePoints, imageSize, cameraMatrix, modelMatrix);
    if(calibrationReady && !previousReady) {
        cout << cameraMatrix << endl;
        cout << modelMatrix << endl;
    }
}

void ofApp::draw(){
    if (meshView) {
        drawMeshView();
    } else {
        drawImageView();
    }
}

void ofApp::drawMeshView() {
    camera.begin();
    ofPushStyle();
    {
        ofSetColor(255,1);
        mesh.draw(); // draw mesh for grabcam depth testing
        ofSetColor(255);
        ofDisableDepthTest();
        mesh.drawWireframe(); // draw wireframe for visualization
        ofSetColor(ofxCv::cyanPrint);
        mesh.drawVertices(); // draw points for selection
    }
    ofPopStyle();
    camera.end();
    
    ofNoFill();
    
    // project points from mesh to 2d
    vector<ofVec3f> points = projectPoints(camera, mesh);
    
    // find the closest point to the mouse and return its index and position
    ofVec2f mouse(mouseX, mouseY);
    hoveringIndex = pickPoints(points, mouse);
    hoveringPosition = points[hoveringIndex];
    
    // draw the hovering or selected point
    float distance = mouse.distance(hoveringPosition);
    hovering = distance < radius;
    if (hovering) {
        ofSetColor(ofxCv::cyanPrint);
        ofDrawCircle(hoveringPosition, radius);
    }
    
    // draw additional mapped points
    for (auto pair : mappedIndices) {
        bool highlight = pair.first == selectedIndex && selected;
        ofSetColor(highlight ? ofxCv::yellowPrint : ofxCv::cyanPrint);
        ofDrawCircle(points[pair.first], radius);
    }
}

void ofApp::drawImageView() {
    // draw reference image underlay
    ofPushMatrix();
    float scale = ofGetHeight() / image.getHeight();
    ofScale(scale, scale);
    ofSetColor(255);
    
    ofPopMatrix();
    
    // draw calibrated mesh
//    drawCalibratedMesh();
    
    // find closest points
    if (mappedIndices.empty()) return;
    
    ofVec2f mouse(mouseX, mouseY);
    hoveringIndex = pickPoints(mappedIndices, mouse);
    hoveringPosition = mappedIndices[hoveringIndex];
    
    // check the hovering point radius
    float distance = mouse.distance(hoveringPosition);
    hovering = distance < radius;
    
    // draw selected and mapped points
    for (auto pair : mappedIndices) {
        if (pair.first == selectedIndex && selected) {
            ofSetColor(ofxCv::yellowPrint);
            ofDrawCircle(pair.second, 1);
            // add crosshairs here
        } else if (pair.first == hoveringIndex && hovering) {
            ofSetColor(ofxCv::magentaPrint);
        } else {
            ofSetColor(ofxCv::cyanPrint);
        }
        ofDrawCircle(pair.second, radius);
    }
}

void ofApp::drawCalibratedMesh() {
    if (!calibrationReady) return;
    
    ofPushMatrix();
    ofSetMatrixMode(OF_MATRIX_PROJECTION);
//    ofLoadIdentityMatrix(); // maybe ofViewport() instead?
    ofMultMatrix(cameraMatrix);
    
    ofPushMatrix();
    ofSetMatrixMode(OF_MATRIX_MODELVIEW);
    ofLoadIdentityMatrix();
    ofMultMatrix(modelMatrix);
    
    ofPushStyle();
    ofSetColor(255);
    mesh.drawWireframe();
    ofPopStyle();
    
    ofPopMatrix();
    ofSetMatrixMode(OF_MATRIX_PROJECTION);
    ofPopMatrix();
    ofSetMatrixMode(OF_MATRIX_MODELVIEW);
}

void ofApp::keyPressed(int key) {
    if (key == 'u') {
        camera.toggleFixUpDirectionEnabled();
    }
    if (key == ' ') {
        meshView = !meshView;
        camera.setListenersEnabled(meshView);
    }
    if (key == OF_KEY_BACKSPACE) {
        if (selected && !mappedIndices.empty()) {
            mappedIndices.erase(selectedIndex);
        }
    }
}

void ofApp::mousePressed(int x, int y, int button) {
    bool previousSelected = selected;
    selected = hovering;
    if (selected) {
        selectedPosition = hoveringPosition;
        selectedIndex = hoveringIndex;
        mappedIndices[selectedIndex] = selectedPosition;
    }
}

void ofApp::mouseDragged(int x, int y, int button) {
    if (!meshView && selected) {
        mappedIndices[selectedIndex].set(x,y);
    }
}

#include "ofApp.h"

#include "LightLeaksUtilities.h"

using namespace cv;
using namespace ofxCv;

template <class T>
void removeIslands(ofPixels_<T>& img) {
    int w = img.getWidth(), h = img.getHeight();
    int ia1=-w-1,ia2=-w-0,ia3=-w+1,ib1=-0-1,ib3=-0+1,ic1=+w-1,ic2=+w-0,ic3=+w+1;
    T* p = img.getData();
    for(int y = 1; y + 1 < h; y++) {
        for(int x = 1; x + 1 < w; x++) {
            int i = y * w + x;
            if(p[i]) {
                if(!p[i+ia1]&&!p[i+ia2]&&!p[i+ia3]&&!p[i+ib1]&&!p[i+ib3]&&!p[i+ic1]&&!p[i+ic2]&&!p[i+ic3]) {
                    p[i] = 0;
                }
            }
        }
    }
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
    ofSetLogLevel(OF_LOG_VERBOSE);
    ofSetVerticalSync(true);
    ofSetFrameRate(120);
    
    setCalibrationDataPathRoot();
    xyzShader.load("shader/xyz.vs", "shader/xyz.fs");
    
    confidenceThreshold = 0.01; //0.05; //settings.getFloatValue("buildXyz/confidenceThreshold");
    viewBetternes = 1.0; //settings.getFloatValue("buildXyz/viewBetternes");
    scaleFactor = 4; //settings.getIntValue("buildXyz/scaleFactor");
    
    
    //----- Model stuff
    colors[0] = ofColor(255,0,0);
    colors[1] = ofColor(255,255,0);
    colors[2] = ofColor(100,0,255);
    colors[3] = ofColor(0,255,0);
    colors[4] = ofColor(0,255,255);
    colors[5] = ofColor(0,0,255);
    colors[6] = ofColor(100,255,0);
    colors[7] = ofColor(0,100,255);
    colors[8] = ofColor(255,0,100);
    colors[9] = ofColor(255,0,0);
    
    
    model.loadModel("model.dae");
    objectMesh = model.getMesh(0);
    
    auto center = objectMesh.getCentroid();
    cam.setTarget(center);
    
    ofVec3f min, max;
    getBoundingBox(objectMesh, min, max);
    zero = min;
    ofVec3f diagonal = max - min;
    range = MAX(MAX(diagonal.x, diagonal.y), diagonal.z);
    cout << "Using min " << min << " max " << max << " and range " << range << endl;
    
    
    mesh.enableColors();
    mesh.setMode(OF_PRIMITIVE_POINTS);
    
    referencePointsMesh.enableColors();
    referencePointsMesh.setMode(OF_PRIMITIVE_POINTS);
    
    meshOutput.enableColors();
    meshOutput.setMode(OF_PRIMITIVE_POINTS);
    
    meanPoints.enableColors();
    meanPoints.setMode(OF_PRIMITIVE_POINTS);
    
    ///----
    
    totalFound = false; // True if a scan marked total was found
    
    /*
     
     vector<ofFile> scanNames = getScanNames();
     for(int i = 0; i < scanNames.size(); i++) {
     ofFile scanName = scanNames[i];
     processScan(scanName);
     }
     
     saveResult();
     
     */
    //ofSaveImage(proNormalFinal, "normalMap.exr");
    
    
}

void ofApp::update() {
    
}

void ofApp::draw() {
    ofBackground(128);
    
    ofSetColor(255);
    //debugFbo.draw(0,0,500,500);
    //xyzFbo.draw(500,0,500,500);
    
    cam.begin();
    
    //ofTranslate(-range*0.5,-range*0.25,-range*0.1);
    
    ofSetColor(100,100,100);
    objectMesh.drawWireframe();
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    
    glPointSize(2);
    
    if(ofGetKeyPressed()){
        meshOutput.draw();
    } else {
        mesh.draw();
    }
    ofSetColor(255);
    glPointSize(6);
    referencePointsMesh.draw();
    
    ofPushStyle();
    ofDisableDepthTest();
    glPointSize(32);
    meanPoints.enableColors();
    meanPoints.draw();
    
    glPointSize(16);
    meanPoints.disableColors();
    meanPoints.draw();
    ofPopStyle();
    
    glPointSize(1);
    cam.end();
    
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    
    
    ofSetColor(255, 255, 255);
    ofDrawBitmapString("DRAG SCANS IN FROM SHARED DATA", 10, 20);
    
    ofDrawBitmapString("space - Show output points", 10, 40);
    ofDrawBitmapString("c - Clear", 10, 55);
    ofDrawBitmapString("s - Save output", 10, 70);
    
    ofDrawBitmapString(statusText, 10, 100);
    
    
    
}


void ofApp::dragged(ofDragInfo & drag){
//    statusText = "";
    
    for(int i=0;i<drag.files.size();i++){
        ofLog()<<drag.files[i];
        ofFile f = ofFile(drag.files[i]);
        processScan(f);

    }
    
    meshOutput.clear();
    int w = proXyzCombined.cols, h = proXyzCombined.rows;
    for(int y = 0; y < h; y++) {
        for(int x = 0; x < w; x++) {
            if(!proConfidenceCombined.at<float>(y, x)) {
            } else {
                ofColor c =debugViewOutput.getColor(x,y);
                meshOutput.addColor(c);
                
                Vec4f xyz= proXyzCombined.at<Vec4f>(y,x);
                meshOutput.addVertex(ofVec3f(xyz[0],xyz[1],xyz[2])*range + zero);
            }
        }
    }

}

void ofApp::keyPressed( int key ){
    if(key == 's'){
        saveResult();
        ofLog()<<"Save done";
    }
    if(key=='c'){
        proXyzCombined = Mat::zeros(0,0, CV_32FC4);
        proDistCombined = Mat::zeros(0,0, CV_32FC1);

        
        mesh.clear();
        referencePointsMesh.clear();
        meanPoints.clear();
        meshOutput.clear();
        totalFound = false;
        colorCounter = 0;
    }
}

void ofApp::autoCalibrateXyz(string path, cv::Mat proConfidenceMat, cv::Mat proMapMat){
    ofImage referenceImage;
    referenceImage.load(path+"/maxImage.jpg");
    
    ofFbo::Settings settings;
    settings.width = referenceImage.getWidth()/scaleFactor;
    settings.height = referenceImage.getHeight()/scaleFactor;
    settings.useDepth = true;
    settings.numSamples = 1;
    settings.internalformat = GL_RGBA32F_ARB;
    
    xyzFbo.allocate(settings);
    //normalFbo.allocate(settings);
    debugFbo.allocate(settings);
    
    
    vector<Point3f> referencePoints;
    vector<Point2f> imagePoints;
    
    int w = proXyzCombined.cols, h = proXyzCombined.rows;
    for(int y = 0; y < h; y++) {
        for(int x = 0; x < w; x++) {
            float cur = proConfidenceMat.at<float>(y, x);
            if(cur > 0.1) {
                
                Vec4f xyz;
                if(totalFound){
                    xyz = proXyzTotalCombined.at<Vec4f>(y, x);
                } else {
                    xyz = proXyzCombined.at<Vec4f>(y, x);
                }
                
                //  cout<<x<<"  "<<y<<"   "<<xyz[0]<<" "<<xyz[1]<<" "<<xyz[2]<<endl;
                if(xyz[0] != 0 || xyz[1] != 0  || xyz[2] != 0 ){
                    Vec3w cur = proMapMat.at<Vec3w>(y, x);
                    
                    referencePoints.push_back(Point3f(xyz[0],xyz[1],xyz[2])*range + Point3f(zero.x, zero.y, zero.z));
                    imagePoints.push_back(Point2f(cur[0] / scaleFactor, cur[1] / scaleFactor));
                }
            }
        }
    }
    
    cout<<"Number of matching points: "<<imagePoints.size()<<endl;
    
    // Prepare for camera calibration
    
    
    Size2i imageSize(referenceImage.getWidth()/scaleFactor, referenceImage.getHeight()/scaleFactor);
    
    float aov = 80;
    float f = imageSize.width * ofDegToRad(aov); // i think this is wrong, but it's optimized out anyway
    Point2f c = Point2f(imageSize) * (1. / 2);
    
    
    int flags = CV_CALIB_USE_INTRINSIC_GUESS | CV_CALIB_ZERO_TANGENT_DIST | CV_CALIB_FIX_ASPECT_RATIO | CV_CALIB_FIX_K1 | CV_CALIB_FIX_K2 | CV_CALIB_FIX_K3 | CV_CALIB_FIX_PRINCIPAL_POINT;
    
    // Run calibrate multiple times to find the best match
    __block float bestDistance = -1;
    __block int bestJump=2;
    
    int minPoints = 30;
    int stride = 50;
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0);
    dispatch_apply((imagePoints.size()/minPoints-4)/stride, queue, ^(size_t i) {
        for(int k = 4+i*stride;k<4+i*stride+stride;k++){
            
            // for(int k=4;k<imagePoints.size()/minPoints;k++){
            Mat1d cameraMatrix = (Mat1d(3, 3) <<
                                  f, 0, c.x,
                                  0, f, c.y,
                                  0, 0, 1);
            
            vector<vector<Point3f> >  _referencePoints(1);
            vector<vector<Point2f> > _imagePoints(1);
            vector<Mat> rvecs, tvecs;
            Mat distCoeffs;
            
            _referencePoints[0].clear();
            _imagePoints[0].clear();
            int jump = k;
            for(int j=0;j<imagePoints.size();j+=jump){
                _referencePoints[0].push_back(referencePoints[j]);
                _imagePoints[0].push_back(imagePoints[j]);
            }
            
            calibrateCamera(_referencePoints, _imagePoints, imageSize, cameraMatrix, distCoeffs, rvecs, tvecs, flags);
            cv::Mat rvec = rvecs[0];
            cv::Mat tvec = tvecs[0];
            
            vector<Point2f>  imagePoints2(0);
            projectPoints(_referencePoints[0], rvec, tvec, cameraMatrix, distCoeffs, imagePoints2);
            
            float projectionDistance = 0;
            for(int j=0;j<imagePoints2.size();j++){
                float xx = imagePoints2[j].x - _imagePoints[0][j].x;
                float yy = imagePoints2[j].y - _imagePoints[0][j].y;
                projectionDistance += sqrt(xx*xx + yy*yy);
                //                    cout<<imagePoints2[j]<<"  "<<_imagePoints[0][j]<<endl;
                //cout<<imagePoints2[j]<<"  "<<_imagePoints[0][j]<<"  "<<sqrt(xx*xx + yy*yy)<<"  "<<projectionDistance<<endl;
            }
            projectionDistance /= imagePoints2.size();
            cout<<(floor(100*k/(imagePoints.size()/minPoints)))<<"% - Jump "<<k<<" Projection error with "<<_imagePoints[0].size()<<" points: "<<projectionDistance<<endl;
            
            if(bestDistance == -1 || bestDistance > projectionDistance ){
                bestDistance = projectionDistance;
                bestJump = k;
            }
        }
    });
    
    
    
    cout<<"Best distance "<<bestDistance<<" (jump="<<bestJump<<")"<<endl;
    statusText += "\nAuto Mapping - Number matching: "+ofToString(floorf(imagePoints.size()/bestJump));
    statusText += "\nMatching distance: "+ofToString(bestDistance);
    
    vector<vector<Point3f> >  _referencePoints(1);
    vector<vector<Point2f> > _imagePoints(1);
    vector<Mat> rvecs, tvecs;
    cv::Mat rvec, tvec;
    Mat distCoeffs;
    
    int jump = bestJump;
    
    for(int j=0;j<imagePoints.size();j+=jump){
        _referencePoints[0].push_back(referencePoints[j]);
        _imagePoints[0].push_back(imagePoints[j]);
        
        referencePointsMesh.addColor(ofColor(255,255,255));
        referencePointsMesh.addVertex(ofVec3f(referencePoints[j].x,referencePoints[j].y,referencePoints[j].z));
    }
    
    
    Mat1d cameraMatrix = (Mat1d(3, 3) <<
                          f, 0, c.x,
                          0, f, c.y,
                          0, 0, 1);
    
    calibrateCamera(_referencePoints, _imagePoints, imageSize, cameraMatrix, distCoeffs, rvecs, tvecs, flags);
    rvec = rvecs[0];
    tvec = tvecs[0];
    intrinsics.setup(cameraMatrix, imageSize);
    modelMatrix = makeMatrix(rvec, tvec);
    
    
    vector<Point2f>  imagePoints2;
    projectPoints(_referencePoints[0], rvec, tvec, cameraMatrix, distCoeffs, imagePoints2);
    
    cout<<endl<<endl<<endl;
    
    float projectionDistance = 0;
    for(int j=0;j<imagePoints2.size();j++){
        float xx = imagePoints2[j].x - _imagePoints[0][j].x;
        float yy = imagePoints2[j].y - _imagePoints[0][j].y;
        projectionDistance += sqrt(xx*xx + yy*yy);
        cout<<imagePoints2[j]<<"  "<<_imagePoints[0][j]<<"  "<<sqrt(xx*xx + yy*yy)<<"  "<<projectionDistance<<endl;
    }
    projectionDistance /= imagePoints2.size();
    
    debugFbo.begin();
    ofClear(0);
    ofSetColor(0,0,0);
    ofDrawRectangle(0,0,debugFbo.getWidth(), debugFbo.getHeight());
    ofSetColor(255,255,255);
    referenceImage.draw(0,0,debugFbo.getWidth(), debugFbo.getHeight());
    
    ofNoFill();
    int jj = 0;
    for(int j=0;j<imagePoints.size();j+=jump){
        ofSetColor(255,0,0);
        ofDrawCircle(imagePoints[j].x,imagePoints[j].y,4);
        
        ofSetColor(255,255,0);
        ofDrawCircle(imagePoints2[jj].x,imagePoints2[jj].y,4);
        
        ofSetColor(0,100,100);
        ofDrawLine(imagePoints[j].x,imagePoints[j].y, imagePoints2[jj].x,imagePoints2[jj].y);
        jj++;
    }
    
    
    ofFill();
    
    
    debugFbo.end();
    
    ofPixels debugPix;
    debugFbo.readToPixels(debugPix);
    ofSaveImage(debugPix, path+"/_debug.png");
    
    xyzFbo.begin(); {
        ofClear(0,0,0,255);
        ofSetColor(255,255,255);
        
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushMatrix();
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        /*                    intrinsics.loadProjectionMatrix(10, 2000);
         applyMatrix(modelMatrix);
         
         ofSetColor(100,100,100);
         objectMesh.drawWireframe();
         */
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        
        intrinsics.loadProjectionMatrix(10, 2000);
        applyMatrix(modelMatrix);
        
        xyzShader.begin();
        xyzShader.setUniform1f("range", range);
        xyzShader.setUniform3fv("zero", zero.getPtr());
        objectMesh.drawFaces();
        xyzShader.end();
        
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
    }xyzFbo.end();
    
    ofFloatPixels pix;
    xyzFbo.readToPixels(pix);
    ofSaveImage(pix, path+"/xyzMap.exr");
    
    //normalFbo.readToPixels(pix);
    //ofSaveImage(pix, path+"/_normalMap.exr");
    //normalMap.load(path + "/_normalMap.exr");
    
}

void ofApp::processScan(ofFile scanName){
    string path = scanName.path();
    
    statusText += "\n\nProcess "+scanName.getFileName();

    float confidenceMultiplier = 1.0;
    bool isTotal = false;
    //        if(scanName.getFileName().substr(-7)
    if(scanName.isDirectory() && path[0] != '_') {
        ofLog()<<scanName.getFileName().substr(scanName.getFileName().size()-7);
        if(scanName.getFileName().substr(scanName.getFileName().size()-7) == "lowconf"){
            confidenceMultiplier = 0.5;
        }
        if(scanName.getFileName().substr(scanName.getFileName().size()-5) == "total"){
            isTotal = true;
           // confidenceMultiplier = 0.5;
            totalFound = true;
        }
        
        ofLog()<<"Is total "<<isTotal;
        ofLog()<<"confidenceMultiplier "<<confidenceMultiplier;
        
        ofLogVerbose() << "processing " << path;
        ofFloatImage xyzMap, proConfidence, normalMap;
        ofShortImage proMap;
        
        proConfidence.load(path + "/proConfidence.exr");
        proMap.load(path + "/proMap.png");
        
        Mat proConfidenceMat = toCv(proConfidence);
        Mat proMapMat = toCv(proMap);
        
        xyzMap.load(path + "/xyzMap.exr");
        //normalMap.load(path + "/normalMap.exr");
        
        if(!xyzMap.isAllocated()){
            cout << "No xyzmap for " << scanName.getBaseName() << endl;
            autoCalibrateXyz(path, proConfidenceMat, proMapMat);
            xyzMap.load(path + "/xyzMap.exr");
        }
        
        Mat xyzMapMat = toCv(xyzMap);
        //Mat normalMapMat = toCv(normalMap);
        
        int w = proMapMat.cols, h = proMapMat.rows;
        if(proXyzCombined.cols == 0) {
            proXyzCombined = Mat::zeros(h, w, CV_32FC4);
            proXyzTotalCombined = Mat::zeros(h, w, CV_32FC4);
            //proNormalCombined = Mat::zeros(h, w, CV_32FC4);
            proConfidenceCombined = Mat::zeros(h, w, CV_32FC1);
            debugViewOutput.allocate(w, h, OF_IMAGE_COLOR);
            proDistCombined = Mat::zeros(h, w, CV_32FC1);
        }
        
        // loop through once to compute the mean xyz points weighted by confidence
        ofVec3f pointSum;
        float pointWeights = 0;
        for(int y = 0; y < h; y++) {
            for(int x = 0; x < w; x++) {
                const float& curConfidence = proConfidenceMat.at<float>(y, x);
                if(curConfidence > confidenceThreshold) {
                    const Vec3w& cur = proMapMat.at<Vec3w>(y, x);
                    const Vec4f& xyzNorm = xyzMapMat.at<Vec4f>(cur[1] / scaleFactor, cur[0] / scaleFactor);
                    ofVec3f xyz(xyzNorm[0], xyzNorm[1], xyzNorm[2]);
                    xyz = xyz * range + zero;
                    pointSum += xyz * curConfidence;
                    pointWeights += curConfidence;
                }
            }
        }
        ofVec3f pointMean = pointSum / pointWeights;
        ofLog() << "mean point: " << pointMean;
        meanPoints.addVertex(pointMean);
        meanPoints.addColor(colors[colorCounter%10]);
        
        // loop through again to compute distances from the mean
        Mat dist = Mat::zeros(h, w, CV_32FC1);
        for(int y = 0; y < h; y++) {
            for(int x = 0; x < w; x++) {
                const float& curConfidence = proConfidenceMat.at<float>(y, x);
                if(curConfidence > confidenceThreshold) {
                    const Vec3w& cur = proMapMat.at<Vec3w>(y, x);
                    const Vec4f& xyzNorm = xyzMapMat.at<Vec4f>(cur[1] / scaleFactor, cur[0] / scaleFactor);
                    ofVec3f xyz(xyzNorm[0], xyzNorm[1], xyzNorm[2]);
                    xyz = xyz * range + zero;
                    dist.at<float>(y, x) = xyz.distance(pointMean);
                }
            }
        }
        
        // resolve these confidences and distances relative to all the others
        for(int y = 0; y < h; y++) {
            for(int x = 0; x < w; x++) {
                const float& curConfidence = proConfidenceMat.at<float>(y, x) * confidenceMultiplier;
                
                if(curConfidence > confidenceThreshold) {
                    const float& curDist = dist.at<float>(y, x);
                    const Vec3w& cur = proMapMat.at<Vec3w>(y, x); // this is just 2d
                    const Vec4f& xyzNorm = xyzMapMat.at<Vec4f>(cur[1] / scaleFactor, cur[0] / scaleFactor); // this is just 3d
                    
                    float& confidenceCombined = proConfidenceCombined.at<float>(y, x);
                    float& combinedDist = proDistCombined.at<float>(y, x);
                    
                    // one way to rectify this is to only use a pixel if the confidence is also better
                    // within a threshold than the previous confidence
//                    if(curConfidence > confidenceCombined * viewBetternes) {
                    if(combinedDist == 0 || (curDist > 0 && curDist < combinedDist)) {
                        combinedDist = curDist;
                        confidenceCombined = curConfidence;
                        proXyzCombined.at<Vec4f>(y, x) = xyzNorm;
                        if(isTotal){
                            proXyzTotalCombined.at<Vec4f>(y, x) = xyzNorm;
                        }
                        //proNormalCombined.at<Vec4f>(y, x) = normalMapMat.at<Vec4f>(cur[1] / scaleFactor, cur[0] / scaleFactor);
                        
                        ofColor c = colors[colorCounter%10];
                        debugViewOutput.setColor(x,y,c);
                    }
                
                    ofVec3f xyz(xyzNorm[0], xyzNorm[1], xyzNorm[2]);
                    xyz = xyz * range + zero;
                    mesh.addVertex(xyz);
                    mesh.addColor(colors[colorCounter%10]);
                }
            }
            
        }
        
    }
    colorCounter++;
}

void ofApp::saveResult(){
    int w = proXyzCombined.cols, h = proXyzCombined.rows;
    
    ofLogVerbose() << "processing final results";
    ofFloatPixels proMapFinal, proNormalFinal, proConfidenceFinal;
    
    // set the alpha channel of xyzMap to confidenceMap, reducing lookups in shader later
//    for(int y = 0; y < h; y++) {
//        for(int x = 0; x < w; x++) {
//            const Vec4f& c = proConfidenceCombined.at<Vec4f>(y, x);
//            Vec4f& p = proXyzCombined.at<Vec4f>(y, x);
//            p[4] = c[0];
//        }
//    }
    
    ofImage mask;
    mask.setUseTexture(false);
    mask.load("mask-0.png");
    mask.setImageType(OF_IMAGE_GRAYSCALE);

    Mat maskMat = toCv(mask);
    
//    proXyzCombined &= maskMat;
//    proConfidenceCombined *= maskMat;
    
    toOf(proXyzCombined, proMapFinal);
    //toOf(proNormalCombined, proNormalFinal);
    toOf(proConfidenceCombined, proConfidenceFinal);
    
    removeIslands(proConfidenceFinal);
    
    for(int y = 0; y < h; y++) {
        for(int x = 0; x < w; x++) {
            if(!proConfidenceCombined.at<float>(y, x) || mask.getColor(x, y).r < 128) {
                proXyzCombined.at<Vec4f>(y, x) = Vec4f(0, 0, 0, 0);
                proConfidenceCombined.at<float>(y, x) = 0;
//                proNormalCombined.at<Vec4f>(y, x) = Vec4f(0, 0, 0, 0);
            } else {
                ofColor c =debugViewOutput.getColor(x,y);
                meshOutput.addColor(c);
                
                Vec4f xyz= proXyzCombined.at<Vec4f>(y,x);
                meshOutput.addVertex(ofVec3f(xyz[0],xyz[1],xyz[2])*range + zero);
            }
        }
    }

    ofLogVerbose() << "saving images";
    ofSaveImage(proConfidenceFinal, "confidenceMap-0.exr");
    ofSaveImage(proMapFinal, "xyzMap-0.exr");
    ofSaveImage(debugViewOutput, "BuildXYZDebug.jpg", OF_IMAGE_QUALITY_BEST);
}

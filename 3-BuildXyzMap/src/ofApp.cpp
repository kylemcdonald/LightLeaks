#include "ofApp.h"

#include "LightLeaksUtilities.h"

#define NUM_REFERENCE_POINTS 130

using namespace cv;
using namespace ofxCv;

template <class T>
void removeIslands(ofPixels_<T>& img) {
	int w = img.getWidth(), h = img.getHeight();
	int ia1=-w-1,ia2=-w-0,ia3=-w+1,ib1=-0-1,ib3=-0+1,ic1=+w-1,ic2=+w-0,ic3=+w+1;
	T* p = img.getPixels();
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
    
    xyzShader.load("xyz.vs", "xyz.fs");
    normalShader.load("normal.vs", "normal.fs");

    
    setCalibrationDataPathRoot();

    
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

    
    ///----
	
	float threshold = 0.0;
	int scaleFactor = 4;
	
	
	vector<ofFile> scanNames = getScanNames();
	for(int i = 0; i < scanNames.size(); i++) {
		ofFile scanName = scanNames[i];
		string path = scanName.path();
		if(scanName.isDirectory() && path[0] != '_') {
			ofLogVerbose() << "processing " << path;
			ofFloatImage xyzMap, proConfidence, normalMap;
			ofShortImage proMap;
			
            
            proConfidence.loadImage(path + "/proConfidence.exr");
            proMap.loadImage(path + "/proMap.png");
            
            Mat proConfidenceMat = toCv(proConfidence);
            Mat proMapMat = toCv(proMap);

            xyzMap.loadImage(path + "/xyzMap.exr");
            normalMap.loadImage(path + "/normalMap.exr");

            if(!xyzMap.isAllocated()){
                cout<<"No xyzmap for "<<scanName<<endl;
                
                ofImage referenceImage;
                referenceImage.loadImage(path+"/maxImage.png");
                
                ofFbo::Settings settings;
                settings.width = referenceImage.getWidth()/scaleFactor;
                settings.height = referenceImage.getHeight()/scaleFactor;
                settings.useDepth = true;
                settings.internalformat = GL_RGBA32F_ARB;

                xyzFbo.allocate(settings);
                normalFbo.allocate(settings);
                debugFbo.allocate(settings);
                
                
                vector<Point3f> referencePoints;
                vector<Point2f> imagePoints;
                
                int w = proXyzCombined.cols, h = proXyzCombined.rows;
                for(int y = 0; y < h; y++) {
                    for(int x = 0; x < w; x++) {
                        float cur = proConfidenceMat.at<float>(y, x);
                        if(cur > 0.5) {
                            Vec4f xyz = proXyzCombined.at<Vec4f>(y, x);
                            
                            if(xyz[0] != 0 || xyz[1] != 0  || xyz[2] != 0 ){
                                Vec3w cur = proMapMat.at<Vec3w>(y, x);
                                //cout<<x<<"  "<<y<<"   "<<xyz[0]<<" "<<xyz[1]<<" "<<xyz[2]<<" --- "<<cur[0] / scaleFactor<<" "<<cur[1] / scaleFactor<<endl;

                                referencePoints.push_back(Point3f(xyz[0],xyz[1],xyz[2])*range);
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
                
                
                int flags = CV_CALIB_USE_INTRINSIC_GUESS | CV_CALIB_ZERO_TANGENT_DIST | CV_CALIB_FIX_ASPECT_RATIO | CV_CALIB_FIX_K1 | CV_CALIB_FIX_K2 | CV_CALIB_FIX_K3;
                
                // Run calibrate multiple times to find the best match
                __block float bestDistance = -1;
                __block int bestJump=2;
                
                int minPoints = 10;
                int stride = 100;
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
                        cv::Mat rvec, tvec;
                        Mat distCoeffs;
                        
                        _referencePoints[0].clear();
                        _imagePoints[0].clear();
                        int jump = k;
                        for(int j=0;j<imagePoints.size();j+=jump){
                            _referencePoints[0].push_back(referencePoints[j]);
                            _imagePoints[0].push_back(imagePoints[j]);
                        }
                        
                        
                        calibrateCamera(_referencePoints, _imagePoints, imageSize, cameraMatrix, distCoeffs, rvecs, tvecs, flags);
                        rvec = rvecs[0];
                        tvec = tvecs[0];
                        intrinsics.setup(cameraMatrix, imageSize);
                        modelMatrix = makeMatrix(rvec, tvec);
                        
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
                        
                        if(bestDistance == -1 || bestDistance > projectionDistance){
                            bestDistance = projectionDistance;
                            bestJump = k;
                        }
                    }
                });
                
                
                
                cout<<"Best distance "<<bestDistance<<" (jump="<<bestJump<<")"<<endl;
                
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
                cout<<"Final Projection error with "<<_imagePoints[0].size()<<" points: "<<projectionDistance<<endl;
                
                
                debugFbo.begin();
                ofClear(0);
                ofSetColor(0,0,0);
                ofRect(0,0,debugFbo.getWidth(), debugFbo.getHeight());
                ofSetColor(255,255,255);
                referenceImage.draw(0,0,debugFbo.getWidth(), debugFbo.getHeight());
                
                ofNoFill();
                int jj = 0;
                for(int j=0;j<imagePoints.size();j+=jump){
                    ofSetColor(255,0,0);
                    ofCircle(imagePoints[j].x,imagePoints[j].y,4);

                    ofSetColor(255,255,0);
                    ofCircle(imagePoints2[jj].x,imagePoints2[jj].y,4);
                    
                    ofSetColor(0,100,100);
                    ofLine(imagePoints[j].x,imagePoints[j].y, imagePoints2[jj].x,imagePoints2[jj].y);
                    jj++;
                }
             
                
                ofFill();

                
                debugFbo.end();

                ofPixels debugPix;
                debugFbo.readToPixels(debugPix);
                ofSaveImage(debugPix, path+"/_debug.png");

                
                normalFbo.begin();{
                    ofClear(0,0,0,255);
                    ofSetColor(255,255,255);
                    
                    glPushAttrib(GL_ALL_ATTRIB_BITS);
                    glPushMatrix();
                    glMatrixMode(GL_PROJECTION);
                    glPushMatrix();
                    glMatrixMode(GL_MODELVIEW);

                    glEnable(GL_DEPTH_TEST);
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_BACK);
                    
                    intrinsics.loadProjectionMatrix(10, 2000);
                    applyMatrix(modelMatrix);
                    
                    normalShader.begin();
                    objectMesh.drawFaces();
                    normalShader.end();
                    
                    glDisable(GL_DEPTH_TEST);
                    glDisable(GL_CULL_FACE);
                } normalFbo.end();
                
                
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
                ofSaveImage(pix, path+"/_xyzMap.exr");
                normalFbo.readToPixels(pix);
                ofSaveImage(pix, path+"/_normalMap.exr");

                xyzMap.loadImage(path + "/_xyzMap.exr");
                normalMap.loadImage(path + "/_normalMap.exr");
            }
            
            
            Mat xyzMapMat = toCv(xyzMap);
            Mat normalMapMat = toCv(normalMap);
            
            if(proXyzCombined.cols == 0) {
                proXyzCombined = Mat::zeros(proMapMat.rows, proMapMat.cols, CV_32FC4);
                proNormalCombined = Mat::zeros(proMapMat.rows, proMapMat.cols, CV_32FC4);
                proConfidenceCombined = Mat::zeros(proConfidenceMat.rows, proConfidenceMat.cols, CV_32FC1);
            }
            
            int w = proXyzCombined.cols, h = proXyzCombined.rows;
            for(int y = 0; y < h; y++) {
                for(int x = 0; x < w; x++) {
                    float cur = proConfidenceMat.at<float>(y, x);
                    if(cur > proConfidenceCombined.at<float>(y, x) && cur > threshold) {
                        proConfidenceCombined.at<float>(y, x) = proConfidenceMat.at<float>(y, x);
                        Vec3w cur = proMapMat.at<Vec3w>(y, x);
                        Vec4f xyz = xyzMapMat.at<Vec4f>(cur[1] / scaleFactor, cur[0] / scaleFactor);
                        proXyzCombined.at<Vec4f>(y, x) = xyz;
                        proNormalCombined.at<Vec4f>(y, x) = normalMapMat.at<Vec4f>(cur[1] / scaleFactor, cur[0] / scaleFactor);
                        
                        mesh.addColor(colors[i%10]);
                        mesh.addVertex(ofVec3f(xyz[0],xyz[1],xyz[2])*range);
                    }
                }
                
            }
		}
	}
	
	ofLogVerbose() << "saving results";
	ofFloatPixels proMapFinal, proNormalFinal, proConfidenceFinal;
	toOf(proXyzCombined, proMapFinal);
	toOf(proNormalCombined, proNormalFinal);
	toOf(proConfidenceCombined, proConfidenceFinal);
	
	removeIslands(proConfidenceFinal);
	ofSaveImage(proConfidenceFinal, "confidenceMap.exr");
	
	ofxCv::threshold(proConfidenceFinal, 0);
	int w = proXyzCombined.cols, h = proXyzCombined.rows;
	for(int y = 0; y < h; y++) {
		for(int x = 0; x < w; x++) {
			if(!proConfidenceCombined.at<float>(y, x)) {
				proXyzCombined.at<Vec4f>(y, x) = Vec4f(0, 0, 0, 0);
				proNormalCombined.at<Vec4f>(y, x) = Vec4f(0, 0, 0, 0);
            }
		}
	}
	ofSaveImage(proMapFinal, "xyzMap.exr");
	ofSaveImage(proNormalFinal, "normalMap.exr");
    

}

void ofApp::update() {
	
}

void ofApp::draw() {
    ofBackground(128);

    ofSetColor(255);
    debugFbo.draw(0,0,500,500);

    
    
    /*xyzFbo.begin(); {
        //                  ofSetColor(0,0,0);
        //                    ofRect(0,0,xyzFbo.getWidth(), xyzFbo.getHeight());
        ofClear(0,0,0,255);
        ofSetColor(255,255,255);
        
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushMatrix();
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        intrinsics.loadProjectionMatrix(10, 2000);
        applyMatrix(modelMatrix);
        
         cam.begin();
        
        ofSetColor(100,100,100);
        objectMesh.drawWireframe();
        
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        
        //   intrinsics.loadProjectionMatrix(10, 2000);
        // applyMatrix(modelMatrix);
        
        
               glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        mesh.draw();

          cam.end();
        ofSetColor(255, 255, 255);
        //ofLine(0,0,100,0);
        
        
    }xyzFbo.end();
*/
    
    
    xyzFbo.draw(500,0,500,500);
    
    
    
    cam.begin();
    
    ofTranslate(-range*0.5,-range*0.25,-range*0.1);
    
    ofSetColor(100,100,100);
    objectMesh.drawWireframe();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

//   intrinsics.loadProjectionMatrix(10, 2000);
  //  applyMatrix(modelMatrix);

    
  /* xyzShader.begin();
    xyzShader.setUniform1f("range", range);
    xyzShader.setUniform3fv("zero", zero.getPtr());
    objectMesh.drawFaces();
    xyzShader.end();*/
    
    glPointSize(3);

    mesh.draw();
    ofSetColor(255);
    glPointSize(8);
    referencePointsMesh.draw();
    glPointSize(1);
    cam.end();
    
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);


}

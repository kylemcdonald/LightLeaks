#include "ofxProCamToolkit.h"
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace ofxCv;
using namespace cv;

void getRemapPoints(string filename, int width, int height, vector<Point2f>& camImagePoints, vector<Point2f>& proImagePoints, vector<unsigned char>& colors, GrayCodeMode mode) {
	Mat codex, codey, camMask;
	Mat cam;
	grayDecode(filename + "vertical/", codex, cam, mode);
	grayDecode(filename + "horizontal/", codey, cam, mode);
	cv::threshold(cam, camMask, 0, 255, CV_THRESH_OTSU);
	Mat remap = buildRemap(codex, codey, camMask, width, height);
	for(int py = 0; py < height; py++) {
		for(int px = 0; px < width; px++) {
			Point2f camPoint = remap.at<Point2f>(py, px);
			if(camPoint != Point2f(0, 0)) {
				camImagePoints.push_back(camPoint);
				proImagePoints.push_back(Point2f(px, py));
				colors.push_back(cam.at<unsigned char>(camPoint));
			}
		}
	}
}

void getProCamImages(string filename, Mat& pro, Mat& cam, int width, int height, GrayCodeMode mode) {
	ofLogVerbose() << "getProCamImages()";
	Mat codex, codey, mask;
	grayDecode(filename + "vertical/", codex, cam, mode);
	grayDecode(filename + "horizontal/", codey, cam, mode);
	cv::threshold(cam, mask, 0, 255, CV_THRESH_OTSU);
	Mat remap = buildRemap(codex, codey, mask, width, height);
	// closing remap gives better results than closing pro
	Mat kernel(3, 3, CV_8U, cv::Scalar(1));
	morphologyEx(remap, remap, cv::MORPH_CLOSE, kernel);
	applyRemap(remap, cam, pro, width, height);
	medianBlur(pro, 3);
}

void grayDecode(string path, Mat& binaryCoded, Mat& cam, GrayCodeMode mode) {
	ofLogVerbose() << "grayDecode()";
	vector<Mat> thresholded;
	Mat minMat, maxMat;
	int n;
	if(mode == GRAYCODE_MODE_GRAY) {
		ofDirectory dir;
		dir.listDir(path);
		n = dir.size();
		thresholded.resize(n);
		ofImage image;
		/*
		ofImage gray;
		ofLogVerbose() << "loading gray";
		gray.load(path + "../0.jpg");
		gray.setImageType(OF_IMAGE_GRAYSCALE);
		Mat grayMat = toCv(gray);
		*/
		for(int i = 0; i < n; i++) {
			ofLogVerbose() << "loading " << dir.getPath(i);
			image.load(dir.getPath(i));
			image.setImageType(OF_IMAGE_GRAYSCALE);
			Mat cur = toCv(image);
			imitate(cam, image);
			
			//thresholded[i] = cur > grayMat;
			cv::threshold(cur, thresholded[i], 0, 255, CV_THRESH_OTSU);
			
			if(i == 0) {
				cur.copyTo(minMat);
				cur.copyTo(maxMat);
			} else {
				cv::min(cur, minMat, minMat);
				cv::max(cur, maxMat, maxMat);
			}
		}
	} else {
		ofDirectory dirNormal, dirInverse;
		dirNormal.listDir(path + "normal/");
		dirInverse.listDir(path + "inverse/");
		n = dirNormal.size();
		thresholded.resize(n);
		ofImage imageNormal, imageInverse;
		for(int i = 0; i < n; i++) {
			imageNormal.load(dirNormal.getPath(i));
			imageInverse.load(dirInverse.getPath(i));
			imageNormal.setImageType(OF_IMAGE_GRAYSCALE);
			imageInverse.setImageType(OF_IMAGE_GRAYSCALE);
			imitate(cam, imageNormal);
			Mat curNormal = toCv(imageNormal);
			Mat curInverse = toCv(imageInverse);
			thresholded[i] = curNormal > curInverse;
			if(i == 0) {
				minMat = min(curNormal, curInverse);
				maxMat = max(curNormal, curInverse);
			} else {
				min(minMat, curNormal, minMat);
				min(minMat, curInverse, minMat);
				max(maxMat, curNormal, maxMat);
				max(maxMat, curInverse, maxMat);
			}
		}
	}
	max(maxMat, cam, cam);
	thresholdedToBinary(thresholded, binaryCoded);	
	grayToBinary(binaryCoded, n);
}

void thresholdedToBinary(vector<Mat>& thresholded, Mat& binaryCoded) {
	ofLogVerbose() << "thresholdedToBinary()";
	int rows = thresholded[0].rows;
	int cols = thresholded[0].cols;
	int n = thresholded.size();
	int m = rows * cols;
	int tw = thresholded[0].cols;
	int th = thresholded[0].rows;
	// create() doesn't clear the memory for some reason
	binaryCoded = Mat::zeros(th, tw, CV_16UC1);
	unsigned short* binaryCodedPixels = binaryCoded.ptr<unsigned short>();
	for(int i = 0; i < n; i++) {
		unsigned char* curThresholdPixels = thresholded[i].ptr<unsigned char>();
		unsigned short curMask = 1 << (n - i - 1);
		for(int k = 0; k < m; k++) {
			if(curThresholdPixels[k]) {
				binaryCodedPixels[k] |= curMask;
			}
		}
	}
}

void grayToBinary(Mat& binaryCoded, int n) {
	unsigned short* binaryCodedPixels = binaryCoded.ptr<unsigned short>();
	
	// built gray code to binary LUT
	int codes = 1 << n;
	vector<unsigned short> binaryLUT(codes);
	for(unsigned short binary = 0; binary < codes; binary++) {
		unsigned short gray = (binary >> 1) ^ binary;
		binaryLUT[gray] = binary;
	}
	
	// convert gray code to binary using LUT
	int m = binaryCoded.rows * binaryCoded.cols;
	for(int i = 0; i < m; i++) {
		binaryCodedPixels[i] = binaryLUT[binaryCodedPixels[i]];
	}
}

Mat buildRemap(Mat& binaryCodedX, Mat& binaryCodedY, Mat& mask, int tw, int th) {
	int rows = binaryCodedX.rows;
	int cols = binaryCodedX.cols;
	Mat remap(th, tw, CV_32FC2);
	Mat total(th, tw, CV_32FC2);
	for(int row = 0; row < rows; row++) {
		for(int col = 0; col < cols; col++) {
			unsigned char curMask = mask.at<unsigned char>(row, col);
			if(curMask) {
				unsigned short tx = binaryCodedX.at<unsigned short>(row, col);
				unsigned short ty = binaryCodedY.at<unsigned short>(row, col);
				unsigned short rtx = MIN(MAX(tx, 0), tw - 1);
				unsigned short rty = MIN(MAX(ty, 0), th - 1);
				if(rtx == tx && rty == ty) {
					remap.at<Point2f>(ty, tx) += Point2f(col, row);
					total.at<Point2f>(ty, tx) += Point2f(1, 1);
				}
			}
		}
	}
	divide(remap, total, remap);
	return remap;
}

void applyRemap(Mat& remap, Mat& input, Mat& output, int width, int height) {
	output.create(height, width, input.type());
	for(int y = 0; y < height; y++) {
		for(int x = 0; x < width; x++) {
			Point2f cur = remap.at<Point2f>(y, x);
			// could be color image (Vec3b) instead
			output.at<unsigned char>(y, x) = input.at<unsigned char>(cur.y, cur.x);
		}
	}
}


void drawChessboardCorners(cv::Size patternSize, const vector<Point2f>& centers) {
	ofMesh lines;
	ofMesh crosses;
	lines.setMode(OF_PRIMITIVE_LINE_STRIP);
	crosses.setMode(OF_PRIMITIVE_LINES);
	int i = 0;
	float radius = 4;
	ofVec2f firstCross = ofVec2f(0, radius).getRotated(-45);
	ofVec2f secondCross = ofVec2f(0, radius).getRotated(+45);
	ofNoFill();
	for(int y = 0; y < patternSize.height; y++) {
		for(int x = 0; x < patternSize.width; x++) {
			if(i == centers.size()) {
				return;
			}
			ofVec2f cur = toOf(centers[i++]);
			ofDrawCircle(cur, radius);
			crosses.addVertex(ofVec3f(cur) - firstCross);
			crosses.addVertex(ofVec3f(cur) + firstCross);
			crosses.addVertex(ofVec3f(cur) - secondCross);
			crosses.addVertex(ofVec3f(cur) + secondCross);
			lines.addVertex(ofVec3f(cur));
		}
	}
	lines.draw();
	crosses.draw();
}

vector<vector<Point3f> > buildObjectPoints(cv::Size patternSize, float squareSize, int objectCount, CalibrationPattern patternType) {
	vector<Point3f> objectPoints = Calibration::createObjectPoints(patternSize, squareSize, patternType);
	vector<vector<Point3f> > results;
	results.resize(objectCount, objectPoints);
	return results;
}

// need to use undistorted camera matrix when calling undistort points?
vector<Point3f> triangulatePositions(
		vector<Point2f>& camImagePoints, Mat camMatrix, Mat camDistCoeffs,
		vector<Point2f>& proImagePoints, Mat proMatrix, Mat proDistCoeffs,
		Mat proRotation, Mat proTranslation) {
	vector<Point3f> result;
	Mat undistCamPoints, undistProPoints;
	undistortPoints(Mat(camImagePoints), undistCamPoints, camMatrix, camDistCoeffs);
	undistortPoints(Mat(proImagePoints), undistProPoints, proMatrix, proDistCoeffs);
	vector<Point2f> camPoints = (vector<Point2f>) undistCamPoints;
	vector<Point2f> proPoints = (vector<Point2f>) undistProPoints;
	Mat camCenter(Point3d(0, 0, 0));
	Mat proCenter(proTranslation);
	for(int j = 0; j < camPoints.size(); j++) {
		Mat camHomogenous(Point3d(camPoints[j].x, camPoints[j].y, 1));
		Mat proHomogenous(Point3d(proPoints[j].x, proPoints[j].y, 1));
		// normally you would need to multiply by the inverse of the cam and pro matrices
		// but that's not necessary because the coordinates have already been normalized by undistortPoints()
		Mat camWorld(camHomogenous);
		Mat proWorld(proRotation * proHomogenous + proTranslation);
		Point3f closest = intersectLineLine(
			(Point3d) camCenter, (Point3d) camWorld,
			(Point3d) proCenter, (Point3d) proWorld);
		result.push_back(closest);
	}
	return result;
}

void drawCamera(Mat camMatrix, cv::Size size, float scale, ofImage& img) {
	int w = size.width;
	int h = size.height;
	
	Mat camInv = camMatrix.inv();
	// homogenous coordinates
	Mat1d hnw = (Mat1d(3,1) << 0, 0, 1);
	Mat1d hne = (Mat1d(3,1) << w, 0, 1);
	Mat1d hsw = (Mat1d(3,1) << 0, h, 1);
	Mat1d hse = (Mat1d(3,1) << w, h, 1);
	// world coordinates
	Mat1d pnw = camInv * hnw;
	Mat1d pne = camInv * hne;
	Mat1d psw = camInv * hsw;
	Mat1d pse = camInv * hse;
	
	ofPushMatrix();
	ofPushStyle();
	
	ofScale(scale, scale, scale);
	ofMesh c;
	c.setMode(OF_PRIMITIVE_LINES);
	#define av(x,y,z) addVertex(ofVec3f(x,y,z))
	#define amv(x) addVertex(ofVec3f(x(0),x(1),x(2)))
	c.av(0,0,0);
	c.amv(pnw);
	c.av(0,0,0);
	c.amv(pne);
	c.av(0,0,0);
	c.amv(pse);
	c.av(0,0,0);
	c.amv(psw);
	
	c.amv(psw);
	c.amv(pnw);
	c.amv(pnw);
	c.amv(pne);
	c.amv(pne);
	c.amv(pse);
	c.amv(pse);
	c.amv(psw);
	c.draw();
	
	ofMesh im;
	im.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
	#define atc(s,t) addTexCoord(ofVec2f(s,t))
	im.atc(0, 0);
	im.amv(pnw);
	im.atc(w, 0);
	im.amv(pne);
	im.atc(w, h);
	im.amv(pse);
	im.atc(0, h);
	im.amv(psw);
	
	ofSetColor(255);
	img.bind();
	im.draw();
	img.unbind();
	
	ofPopStyle();
	ofPopMatrix();
}

void drawLabeledAxes(float size) {
	ofPushMatrix();
	ofPushStyle();
	ofScale(size, size, size);
	ofSetLineWidth(3);
	ofSetColor(ofColor::red);
	ofDrawLine(0, 0, 0, 1, 0, 0);
	ofDrawBitmapString("+x", 1, 0, 0);
	ofSetColor(ofColor::green);
	ofDrawLine(0, 0, 0, 0, 1, 0);
	ofDrawBitmapString("+y", 0, 1, 0);
	ofSetColor(ofColor::blue);
	ofDrawLine(0, 0, 0, 0, 0, 1);
	ofDrawBitmapString("+z", 0, 0, 1);
	ofPopStyle();
	ofPopMatrix();
}

void drawImagePoints(Mat camMatrix, vector<Point2f>& imagePoints, float scale) {
	ofPushMatrix();
	ofPushStyle();
	ofSetColor(255);
	ofScale(scale, scale, scale);
	Mat camInv = camMatrix.inv();
	for(int i = 0; i < imagePoints.size(); i++) {
		Point2f& curPoint = imagePoints[i];
		Mat1d h = (Mat1d(3,1) << curPoint.x, curPoint.y, 1);
		Mat1d w = camInv * h;
		ofDrawLine(0, 0, 0, w(0), w(1), w(2));
		ofDrawBitmapString(ofToString(i), w(0), w(1), w(2));
	}
	ofPopStyle(); 
	ofPopMatrix();
}

ofMesh drawObjectPoints(vector<Point3f>& points) {
	ofMesh mesh;
	mesh.setMode(OF_PRIMITIVE_POINTS);
	for(int i = 0; i < points.size(); i++) {
		mesh.addVertex(toOf(points[i]));
	}
	return mesh;
}

void drawObjectPoints(vector<Point3f>& points, Mat rotation, Mat translation) {
	ofPushStyle();
	ofSetLineWidth(2);
	if(rotation.rows * rotation.cols == 3) {
		Rodrigues(rotation, rotation);
	}
	for(int i = 0; i < points.size(); i++) {
		Mat1d opt = (Mat1d(3, 1) << points[i].x, points[i].y, points[i].z);
		Mat1d wpt;
		if(!(translation.empty() || rotation.empty())) {
			wpt = rotation * opt;
			wpt += translation;
		} else {
			wpt = opt;
		}
		ofPushMatrix();
		ofTranslate(wpt(0), wpt(1), wpt(2));
		ofNoFill();
		ofDrawCircle(0, 0, 6);
		ofPopMatrix();
	}
	ofPopStyle();
}

void drawCamera(string name, Mat camMatrix, cv::Size size,
		ofImage& image,
		Mat rotation, Mat translation) {
	ofPushMatrix();
	ofPushStyle();
	if(!(translation.empty() || rotation.empty())) {
		applyMatrix(makeMatrix(rotation, translation));
	}
	drawCamera(camMatrix, size, 800, image);
	drawLabeledAxes(128);
	ofSetColor(255);
	ofDrawBitmapString(name, 10, 20);	
	ofPopStyle();
	ofPopMatrix();
}

void drawCamera(string name, Mat camMatrix, cv::Size size,
		vector<Point3f>& objectPoints, Mat objectRotation, Mat objectTranslation,
		vector<Point2f>& imagePoints,
		ofImage& image,
		Mat rotation, Mat translation) {
	ofPushMatrix();
	ofPushStyle();
	if(!(translation.empty() || rotation.empty())) {
		applyMatrix(makeMatrix(rotation, translation));
	}
	drawImagePoints(camMatrix, imagePoints, 1500);
	drawObjectPoints(objectPoints, objectRotation, objectTranslation);
	drawCamera(camMatrix, size, 800, image);
	drawLabeledAxes(128);
	ofSetColor(255);
	ofDrawBitmapString(name, 10, 20);
	ofPopStyle();
	ofPopMatrix();
}


glm::vec3 ofWorldToScreen(glm::vec3 world, glm::mat4 modelviewMatrix, glm::mat4 projectionMatrix, glm::vec4 viewport) {
	GLdouble x, y, z;
	glm::vec3 screen = glm::project(world, modelviewMatrix, projectionMatrix, viewport);
	screen.y = ofGetHeight() - screen.y;
	return screen;
}

glm::vec3 ofScreenToWorld(glm::vec3 screen, glm::mat4 modelviewMatrix, glm::mat4 projectionMatrix, glm::vec4 viewport) {
	screen.y = ofGetHeight() - screen.y;
	glm::vec3 world = glm::unProject(screen, modelviewMatrix, projectionMatrix, viewport);
	return world;
}

ofMesh getProjectedMesh(const ofMesh& mesh, ofCamera & cam) {
	Plane pl[6];
	updateFrustrum(cam, pl);

	ofRectangle vp = ofGetCurrentViewport();
	glm::mat4 modelviewMatrix = cam.getModelViewMatrix();
	glm::mat4 projectionMatrix = cam.getProjectionMatrix();
	glm::vec4 viewport = glm::vec4(vp.x, vp.y, vp.width, vp.height);
	ofMesh projected = mesh;
	vector<ofColor> c;
	for (int i = 0; i < mesh.getNumVertices(); i++) {
		glm::vec3 cur = ofWorldToScreen(mesh.getVerticesPointer()[i], modelviewMatrix, projectionMatrix, viewport);
		cur.z = 0;
		projected.setVertex(i, cur);
		if (pointInFrustum((ofVec3f)mesh.getVerticesPointer()[i], pl)) {
			projected.addColor(ofColor::cyan);
		}
		else {
			projected.addColor(ofColor(0, 0, 0, 0));
		}
	}
	return projected;
}

ofMesh getProjectedMesh(const ofMesh& mesh, glm::mat4x4 modelviewMatrix, glm::mat4x4 projectionMatrix, glm::vec4 viewport) {
	ofMesh projected = mesh;
	for(int i = 0; i < mesh.getNumVertices(); i++) {
		glm::vec3 cur = ofWorldToScreen(mesh.getVerticesPointer()[i], modelviewMatrix, projectionMatrix, viewport);
		cur.z = 0;
		projected.setVertex(i, cur);
		projected.addColor(ofColor::cyan);
	}
	return projected;
}

bool pointInFrustum(ofVec3f &p, Plane pl[6]) {
	for (int i = 0; i < 6; i++) {
		if (pl[i].distance(p) < 0) return false;
	}
	return true;
}

void updateFrustrum(ofCamera & cam, Plane pl[6]) {
	ofRectangle viewport = ofGetCurrentViewport();

	float ratio = viewport.width / viewport.height;
	float nearH = 2 * tan(cam.getFov() / 2.0) * -cam.getNearClip();
	float nearW = nearH * ratio;

	float farH = 2 * tan(cam.getFov() / 2.0) * -cam.getFarClip();
	float farW = farH * ratio;

	ofVec3f p = cam.getPosition();
	ofVec3f l = cam.getLookAtDir();
	ofVec3f u = cam.getUpDir();

	ofVec3f fc = p + l * cam.getFarClip();
	ofVec3f nc = p + l * cam.getNearClip();


	ofVec3f up = cam.getUpDir();
	ofVec3f right = cam.getXAxis();

	ofVec3f ftl = fc + (up * farH / 2) - (right * farW / 2);
	ofVec3f ftr = fc + (up * farH / 2) + (right * farW / 2);
	ofVec3f fbl = fc - (up * farH / 2) - (right * farW / 2);
	ofVec3f fbr = fc - (up * farH / 2) + (right * farW / 2);

	ofVec3f ntl = nc + (up * nearH / 2) - (right * nearW / 2);
	ofVec3f ntr = nc + (up * nearH / 2) + (right * nearW / 2);
	ofVec3f nbl = nc - (up * nearH / 2) - (right * nearW / 2);
	ofVec3f nbr = nc - (up * nearH / 2) + (right * nearW / 2);


	pl[0].set(ntr, ntl, ftl);
	pl[1].set(nbl, nbr, fbr);
	pl[2].set(ntl, nbl, fbl);
	pl[3].set(nbr, ntr, fbr);
	pl[4].set(ntl, ntr, nbr);
	pl[5].set(ftr, ftl, fbl);
}



Point2f getClosestPoint(const vector<Point2f>& vertices, float x, float y, int* choice, float* distance) {
	float bestDistance = numeric_limits<float>::infinity();
	int bestChoice = 0;
	for(int i = 0; i < vertices.size(); i++) {
		const Point2f& cur = vertices[i];
		float dx = x - cur.x;
		float dy = y - cur.y;
		float curDistance = dx * dx + dy * dy;
		if(curDistance < bestDistance) {
			bestDistance = curDistance;
			bestChoice = i;
		}
	}
	if(choice != NULL) {
		*choice = bestChoice;
	}
	if(distance != NULL) {
		*distance = sqrtf(bestDistance);
	}
	return vertices[bestChoice];
}

glm::vec3 getClosestPointOnMesh(const ofMesh& mesh, float x, float y, int* choice, float* distance) {
	float bestDistance = numeric_limits<float>::infinity();
	int bestChoice = 0;
	for(int i = 0; i < mesh.getNumVertices(); i++) {
		const glm::vec3& cur = mesh.getVerticesPointer()[i];
		ofColor col = mesh.getColor(i);
		if (col.a != 0) {
			float dx = x - cur.x;
			float dy = y - cur.y;
			float curDistance = dx * dx + dy * dy;
			if (curDistance < bestDistance) {
				bestDistance = curDistance;
				bestChoice = i;
			}
		}
	}
	if(choice != NULL) {
		*choice = bestChoice;
	}
	if(distance != NULL) {
		*distance = sqrtf(bestDistance);
	}
	return mesh.getVerticesPointer()[bestChoice];
}

int exportPlyVertices(ostream& ply, ofMesh& cloud) {
	int total = 0;
	int i = 0;
	glm::vec3 zero(0, 0, 0);
	vector<ofFloatColor>& colors = cloud.getColors();
    auto& surface = cloud.getVertices();
	for(int i = 0; i < surface.size(); i++) {
		if (glm::vec3(surface[i]) != zero) {
			ply.write(reinterpret_cast<char*>(&(surface[i].x)), sizeof(float));
			ply.write(reinterpret_cast<char*>(&(surface[i].y)), sizeof(float));
			ply.write(reinterpret_cast<char*>(&(surface[i].z)), sizeof(float));
			if(colors.size() > 0) {
				unsigned char color[3] = {
                    (unsigned char) (255 * colors[i].r),
                    (unsigned char) (255 * colors[i].g),
                    (unsigned char) (255 * colors[i].b)};
				ply.write((char*) color, sizeof(char) * 3);
			}
			total++;
		}
	}
	return total;
}

void exportPlyCloud(string filename, ofMesh& cloud) {
	ofstream ply;
	ply.open(ofToDataPath(filename).c_str(), ios::out | ios::binary);
	if (ply.is_open()) {
		// create all the vertices
		stringstream vertices(ios::in | ios::out | ios::binary);
		int total = exportPlyVertices(vertices, cloud);
		
		// write the header
		ply << "ply" << endl;
		ply << "format binary_little_endian 1.0" << endl;
		ply << "element vertex " << total << endl;
		ply << "property float x" << endl;
		ply << "property float y" << endl;
		ply << "property float z" << endl;
		if (cloud.getNumColors() > 0) {
			ply << "property uchar red" << endl;
			ply << "property uchar green" << endl;
			ply << "property uchar blue" << endl;
		}
		ply << "end_header" << endl;
		
		// write all the vertices
		ply << vertices.rdbuf();
	}
}

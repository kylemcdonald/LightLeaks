#include "ofxProCamToolkit.h"


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

GLdouble  modelviewMatrix[16], projectionMatrix[16];
GLint viewport[4];
void updateProjectionState() {
	glGetDoublev(GL_MODELVIEW_MATRIX, modelviewMatrix);
	glGetDoublev(GL_PROJECTION_MATRIX, projectionMatrix);
	glGetIntegerv(GL_VIEWPORT, viewport);
}


ofVec3f ofWorldToScreen(ofVec3f world) {
	updateProjectionState();
	GLdouble  pos[3];
	glhProjectd(world.x, world.y, world.z, modelviewMatrix, projectionMatrix, viewport, pos);
	ofVec3f screen(pos[0], pos[1], pos[2]);
	screen.y = ofGetHeight() - screen.y;
	return screen;
}

ofVec3f ofScreenToWorld(ofVec3f screen) {
	updateProjectionState();
	GLdouble  pos[3];
	screen.y = ofGetHeight() - screen.y;
	glhUnProjectd(screen.x, screen.y, screen.z, modelviewMatrix, projectionMatrix, viewport, pos);
	ofVec3f world(pos[0], pos[1], pos[2]);
	return world;
}

ofMesh getProjectedMesh(const ofMesh& mesh) {
	ofMesh projected = mesh;
	for (int i = 0; i < mesh.getNumVertices(); i++) {
		ofVec3f cur = ofWorldToScreen(mesh.getVerticesPointer()[i]);
		if (cur.z > 1) {
//            ofLog() << "GREATER THAN 1" << endl;
			projected.addColor(ofFloatColor(0, 0, 0, 0));
		}
		else {
			projected.addColor(ofColor::cyan);
		}
		cur.z = 0;
		projected.setVertex(i, cur);
	}
	return projected;
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

ofVec3f getClosestPointOnMesh(const ofMesh& mesh, float x, float y, int* choice, float* distance) {
	float bestDistance = numeric_limits<float>::infinity();
	int bestChoice = 0;
	for(int i = 0; i < mesh.getNumVertices(); i++) {
		const ofVec3f& cur = mesh.getVerticesPointer()[i];
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
	ofVec3f zero(0, 0, 0);
	vector<ofFloatColor>& colors = cloud.getColors();
    auto& surface = cloud.getVertices();
	for(int i = 0; i < surface.size(); i++) {
		if (ofVec3f(surface[i]) != zero) {
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

#define SWAP_ROWS_DOUBLE(a, b) { double *_tmp = a; (a) = (b); (b) = _tmp; }
#define SWAP_ROWS_FLOAT(a, b) { float *_tmp = a; (a) = (b); (b) = _tmp; }
#define MAT(m, r, c) (m)[(c) * 4 + (r)]

// This code comes directly from GLU except that it is for float
int glhInvertMatrixd(double *m, double *out)
{
	double wtmp[4][8];
	double m0, m1, m2, m3, s;
	double *r0, *r1, *r2, *r3;
	r0 = wtmp[0], r1 = wtmp[1], r2 = wtmp[2], r3 = wtmp[3];
	r0[0] = MAT(m, 0, 0), r0[1] = MAT(m, 0, 1),
		r0[2] = MAT(m, 0, 2), r0[3] = MAT(m, 0, 3),
		r0[4] = 1.0, r0[5] = r0[6] = r0[7] = 0.0,
		r1[0] = MAT(m, 1, 0), r1[1] = MAT(m, 1, 1),
		r1[2] = MAT(m, 1, 2), r1[3] = MAT(m, 1, 3),
		r1[5] = 1.0, r1[4] = r1[6] = r1[7] = 0.0,
		r2[0] = MAT(m, 2, 0), r2[1] = MAT(m, 2, 1),
		r2[2] = MAT(m, 2, 2), r2[3] = MAT(m, 2, 3),
		r2[6] = 1.0, r2[4] = r2[5] = r2[7] = 0.0,
		r3[0] = MAT(m, 3, 0), r3[1] = MAT(m, 3, 1),
		r3[2] = MAT(m, 3, 2), r3[3] = MAT(m, 3, 3),
		r3[7] = 1.0, r3[4] = r3[5] = r3[6] = 0.0;
	/* choose pivot - or die */
	if (fabsf(r3[0]) > fabsf(r2[0]))
		SWAP_ROWS_DOUBLE(r3, r2);
	if (fabsf(r2[0]) > fabsf(r1[0]))
		SWAP_ROWS_DOUBLE(r2, r1);
	if (fabsf(r1[0]) > fabsf(r0[0]))
		SWAP_ROWS_DOUBLE(r1, r0);
	if (0.0 == r0[0])
		return 0;
	/* eliminate first variable */
	m1 = r1[0] / r0[0];
	m2 = r2[0] / r0[0];
	m3 = r3[0] / r0[0];
	s = r0[1];
	r1[1] -= m1 * s;
	r2[1] -= m2 * s;
	r3[1] -= m3 * s;
	s = r0[2];
	r1[2] -= m1 * s;
	r2[2] -= m2 * s;
	r3[2] -= m3 * s;
	s = r0[3];
	r1[3] -= m1 * s;
	r2[3] -= m2 * s;
	r3[3] -= m3 * s;
	s = r0[4];
	if (s != 0.0) {
		r1[4] -= m1 * s;
		r2[4] -= m2 * s;
		r3[4] -= m3 * s;
	}
	s = r0[5];
	if (s != 0.0) {
		r1[5] -= m1 * s;
		r2[5] -= m2 * s;
		r3[5] -= m3 * s;
	}
	s = r0[6];
	if (s != 0.0) {
		r1[6] -= m1 * s;
		r2[6] -= m2 * s;
		r3[6] -= m3 * s;
	}
	s = r0[7];
	if (s != 0.0) {
		r1[7] -= m1 * s;
		r2[7] -= m2 * s;
		r3[7] -= m3 * s;
	}
	/* choose pivot - or die */
	if (fabsf(r3[1]) > fabsf(r2[1]))
		SWAP_ROWS_DOUBLE(r3, r2);
	if (fabsf(r2[1]) > fabsf(r1[1]))
		SWAP_ROWS_DOUBLE(r2, r1);
	if (0.0 == r1[1])
		return 0;
	/* eliminate second variable */
	m2 = r2[1] / r1[1];
	m3 = r3[1] / r1[1];
	r2[2] -= m2 * r1[2];
	r3[2] -= m3 * r1[2];
	r2[3] -= m2 * r1[3];
	r3[3] -= m3 * r1[3];
	s = r1[4];
	if (0.0 != s) {
		r2[4] -= m2 * s;
		r3[4] -= m3 * s;
	}
	s = r1[5];
	if (0.0 != s) {
		r2[5] -= m2 * s;
		r3[5] -= m3 * s;
	}
	s = r1[6];
	if (0.0 != s) {
		r2[6] -= m2 * s;
		r3[6] -= m3 * s;
	}
	s = r1[7];
	if (0.0 != s) {
		r2[7] -= m2 * s;
		r3[7] -= m3 * s;
	}
	/* choose pivot - or die */
	if (fabsf(r3[2]) > fabsf(r2[2]))
		SWAP_ROWS_DOUBLE(r3, r2);
	if (0.0 == r2[2])
		return 0;
	/* eliminate third variable */
	m3 = r3[2] / r2[2];
	r3[3] -= m3 * r2[3], r3[4] -= m3 * r2[4],
		r3[5] -= m3 * r2[5], r3[6] -= m3 * r2[6], r3[7] -= m3 * r2[7];
	/* last check */
	if (0.0 == r3[3])
		return 0;
	s = 1.0 / r3[3];		/* now back substitute row 3 */
	r3[4] *= s;
	r3[5] *= s;
	r3[6] *= s;
	r3[7] *= s;
	m2 = r2[3];			/* now back substitute row 2 */
	s = 1.0 / r2[2];
	r2[4] = s * (r2[4] - r3[4] * m2), r2[5] = s * (r2[5] - r3[5] * m2),
		r2[6] = s * (r2[6] - r3[6] * m2), r2[7] = s * (r2[7] - r3[7] * m2);
	m1 = r1[3];
	r1[4] -= r3[4] * m1, r1[5] -= r3[5] * m1,
		r1[6] -= r3[6] * m1, r1[7] -= r3[7] * m1;
	m0 = r0[3];
	r0[4] -= r3[4] * m0, r0[5] -= r3[5] * m0,
		r0[6] -= r3[6] * m0, r0[7] -= r3[7] * m0;
	m1 = r1[2];			/* now back substitute row 1 */
	s = 1.0 / r1[1];
	r1[4] = s * (r1[4] - r2[4] * m1), r1[5] = s * (r1[5] - r2[5] * m1),
		r1[6] = s * (r1[6] - r2[6] * m1), r1[7] = s * (r1[7] - r2[7] * m1);
	m0 = r0[2];
	r0[4] -= r2[4] * m0, r0[5] -= r2[5] * m0,
		r0[6] -= r2[6] * m0, r0[7] -= r2[7] * m0;
	m0 = r0[1];			/* now back substitute row 0 */
	s = 1.0 / r0[0];
	r0[4] = s * (r0[4] - r1[4] * m0), r0[5] = s * (r0[5] - r1[5] * m0),
		r0[6] = s * (r0[6] - r1[6] * m0), r0[7] = s * (r0[7] - r1[7] * m0);
	MAT(out, 0, 0) = r0[4];
	MAT(out, 0, 1) = r0[5], MAT(out, 0, 2) = r0[6];
	MAT(out, 0, 3) = r0[7], MAT(out, 1, 0) = r1[4];
	MAT(out, 1, 1) = r1[5], MAT(out, 1, 2) = r1[6];
	MAT(out, 1, 3) = r1[7], MAT(out, 2, 0) = r2[4];
	MAT(out, 2, 1) = r2[5], MAT(out, 2, 2) = r2[6];
	MAT(out, 2, 3) = r2[7], MAT(out, 3, 0) = r3[4];
	MAT(out, 3, 1) = r3[5], MAT(out, 3, 2) = r3[6];
	MAT(out, 3, 3) = r3[7];
	return 1;
}

int glhProjectd(double objx, double objy, double objz, double *modelview, double *projection, int *viewport, double *windowCoordinate)
{
	// Transformation vectors
	float fTempo[8];
	// Modelview transform
	fTempo[0] = modelview[0] * objx + modelview[4] * objy + modelview[8] * objz + modelview[12]; // w is always 1
	fTempo[1] = modelview[1] * objx + modelview[5] * objy + modelview[9] * objz + modelview[13];
	fTempo[2] = modelview[2] * objx + modelview[6] * objy + modelview[10] * objz + modelview[14];
	fTempo[3] = modelview[3] * objx + modelview[7] * objy + modelview[11] * objz + modelview[15];
	// Projection transform, the final row of projection matrix is always [0 0 -1 0]
	// so we optimize for that.
	fTempo[4] = projection[0] * fTempo[0] + projection[4] * fTempo[1] + projection[8] * fTempo[2] + projection[12] * fTempo[3];
	fTempo[5] = projection[1] * fTempo[0] + projection[5] * fTempo[1] + projection[9] * fTempo[2] + projection[13] * fTempo[3];
	fTempo[6] = projection[2] * fTempo[0] + projection[6] * fTempo[1] + projection[10] * fTempo[2] + projection[14] * fTempo[3];
	fTempo[7] = -fTempo[2];
	// The result normalizes between -1 and 1
	if (fTempo[7] == 0.0) // The w value
		return 0;
	fTempo[7] = 1.0 / fTempo[7];
	// Perspective division
	fTempo[4] *= fTempo[7];
	fTempo[5] *= fTempo[7];
	fTempo[6] *= fTempo[7];
	// Window coordinates
	// Map x, y to range 0-1
	windowCoordinate[0] = (fTempo[4] * 0.5 + 0.5)*viewport[2] + viewport[0];
	windowCoordinate[1] = (fTempo[5] * 0.5 + 0.5)*viewport[3] + viewport[1];
	// This is only correct when glDepthRange(0.0, 1.0)
	windowCoordinate[2] = (1.0 + fTempo[6])*0.5;	// Between 0 and 1
	return 1;
}

int glhUnProjectd(double winx, double winy, double winz, double *modelview, double *projection, int *viewport, double *objectCoordinate)
{
	// Transformation matrices
	double m[16], A[16];
	double in[4], out[4];
	// Calculation for inverting a matrix, compute projection x modelview
	// and store in A[16]
	MultiplyMatrices4by4OpenGL_DOUBLE(A, projection, modelview);
	// Now compute the inverse of matrix A
	if (glhInvertMatrixd(A, m) == 0)
		return 0;
	// Transformation of normalized coordinates between -1 and 1
	in[0] = (winx - (double)viewport[0]) / (double)viewport[2] * 2.0 - 1.0;
	in[1] = (winy - (double)viewport[1]) / (double)viewport[3] * 2.0 - 1.0;
	in[2] = 2.0*winz - 1.0;
	in[3] = 1.0;
	// Objects coordinates
	MultiplyMatrices4by4OpenGL_DOUBLE(out, m, in);
	if (out[3] == 0.0)
		return 0;
	out[3] = 1.0 / out[3];
	objectCoordinate[0] = out[0] * out[3];
	objectCoordinate[1] = out[1] * out[3];
	objectCoordinate[2] = out[2] * out[3];
	return 1;
}

void MultiplyMatrices4by4OpenGL_DOUBLE(double *result, double *matrix1, double *matrix2)
{
	result[0] = matrix1[0] * matrix2[0] +
		matrix1[4] * matrix2[1] +
		matrix1[8] * matrix2[2] +
		matrix1[12] * matrix2[3];
	result[4] = matrix1[0] * matrix2[4] +
		matrix1[4] * matrix2[5] +
		matrix1[8] * matrix2[6] +
		matrix1[12] * matrix2[7];
	result[8] = matrix1[0] * matrix2[8] +
		matrix1[4] * matrix2[9] +
		matrix1[8] * matrix2[10] +
		matrix1[12] * matrix2[11];
	result[12] = matrix1[0] * matrix2[12] +
		matrix1[4] * matrix2[13] +
		matrix1[8] * matrix2[14] +
		matrix1[12] * matrix2[15];
	result[1] = matrix1[1] * matrix2[0] +
		matrix1[5] * matrix2[1] +
		matrix1[9] * matrix2[2] +
		matrix1[13] * matrix2[3];
	result[5] = matrix1[1] * matrix2[4] +
		matrix1[5] * matrix2[5] +
		matrix1[9] * matrix2[6] +
		matrix1[13] * matrix2[7];
	result[9] = matrix1[1] * matrix2[8] +
		matrix1[5] * matrix2[9] +
		matrix1[9] * matrix2[10] +
		matrix1[13] * matrix2[11];
	result[13] = matrix1[1] * matrix2[12] +
		matrix1[5] * matrix2[13] +
		matrix1[9] * matrix2[14] +
		matrix1[13] * matrix2[15];
	result[2] = matrix1[2] * matrix2[0] +
		matrix1[6] * matrix2[1] +
		matrix1[10] * matrix2[2] +
		matrix1[14] * matrix2[3];
	result[6] = matrix1[2] * matrix2[4] +
		matrix1[6] * matrix2[5] +
		matrix1[10] * matrix2[6] +
		matrix1[14] * matrix2[7];
	result[10] = matrix1[2] * matrix2[8] +
		matrix1[6] * matrix2[9] +
		matrix1[10] * matrix2[10] +
		matrix1[14] * matrix2[11];
	result[14] = matrix1[2] * matrix2[12] +
		matrix1[6] * matrix2[13] +
		matrix1[10] * matrix2[14] +
		matrix1[14] * matrix2[15];
	result[3] = matrix1[3] * matrix2[0] +
		matrix1[7] * matrix2[1] +
		matrix1[11] * matrix2[2] +
		matrix1[15] * matrix2[3];
	result[7] = matrix1[3] * matrix2[4] +
		matrix1[7] * matrix2[5] +
		matrix1[11] * matrix2[6] +
		matrix1[15] * matrix2[7];
	result[11] = matrix1[3] * matrix2[8] +
		matrix1[7] * matrix2[9] +
		matrix1[11] * matrix2[10] +
		matrix1[15] * matrix2[11];
	result[15] = matrix1[3] * matrix2[12] +
		matrix1[7] * matrix2[13] +
		matrix1[11] * matrix2[14] +
		matrix1[15] * matrix2[15];
}

void MultiplyMatrixByVector4by4OpenGL_FLOAT(double *resultvector, const double *matrix, const double *pvector)
{
	resultvector[0] = matrix[0] * pvector[0] + matrix[4] * pvector[1] + matrix[8] * pvector[2] + matrix[12] * pvector[3];
	resultvector[1] = matrix[1] * pvector[0] + matrix[5] * pvector[1] + matrix[9] * pvector[2] + matrix[13] * pvector[3];
	resultvector[2] = matrix[2] * pvector[0] + matrix[6] * pvector[1] + matrix[10] * pvector[2] + matrix[14] * pvector[3];
	resultvector[3] = matrix[3] * pvector[0] + matrix[7] * pvector[1] + matrix[11] * pvector[2] + matrix[15] * pvector[3];
}


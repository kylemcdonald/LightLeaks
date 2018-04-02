//
//  Plane.h
//  PointsInCamera
//
//  Created by Todd Vanderlin on 11/26/13.
//
//
#pragma once
#include "ofVec3f.h"

//--------------------------------------------------------------
// A 3D Plane
//--------------------------------------------------------------
class Plane {
public:
	ofVec3f p0, p1, p2;
	void set(ofVec3f pt0, ofVec3f pt1, ofVec3f pt2) {
		p0 = pt0; p1 = pt1; p2 = pt2;

	}
	float distance(ofVec3f point) {

		ofVec3f v = p1 - p0;
		ofVec3f u = p2 - p0;
		ofVec3f n = v.cross(u);
		n.normalize();

		float A = n.x;
		float B = n.y;
		float C = n.z;
		float D = -n.dot(p1);

		return (A * point.x) + (B * point.y) + (C * point.z) + D;
	}
	void draw() {
	}
};
//--------------------------------------------------------------

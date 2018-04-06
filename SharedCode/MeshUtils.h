#pragma once
#include "ofGraphicsBaseTypes.h"
#include "ofVectorMath.h"
#include "ofMath.h"
#include "ofLog.h"
#include "ofxAssimpModelLoader.h"


void getBoundingBox(const ofMesh& mesh, glm::vec3& cornerMin, glm::vec3& cornerMax) {
	const vector<glm::vec3>& vertices = mesh.getVertices();
	if(vertices.size() > 0) {
		cornerMin = vertices[0];
		cornerMax = vertices[0];
	}
	for(int i = 0; i < vertices.size(); i++) {
		cornerMin.x = MIN(cornerMin.x, vertices[i].x);
		cornerMin.y = MIN(cornerMin.y, vertices[i].y);
		cornerMin.z = MIN(cornerMin.z, vertices[i].z);
		cornerMax.x = MAX(cornerMax.x, vertices[i].x);
		cornerMax.y = MAX(cornerMax.y, vertices[i].y);
		cornerMax.z = MAX(cornerMax.z, vertices[i].z);
	}
}

void centerAndNormalize(ofMesh& mesh) {
	glm::vec3 cornerMin, cornerMax;
	getBoundingBox(mesh, cornerMin, cornerMax);
	glm::vec3 translate = -(cornerMax + cornerMin) / 2;
	glm::vec3 range = (cornerMax - cornerMin);
	float maxRange = 0;
	maxRange = MAX(maxRange, range.x);
	maxRange = MAX(maxRange, range.y);
	maxRange = MAX(maxRange, range.z);
	float scale = 1 / maxRange;
	vector<glm::vec3>& vertices = mesh.getVertices();
	for(int i = 0; i < vertices.size(); i++) {
		vertices[i] += translate;
		vertices[i] *= scale;
	}
}

void drawNormals(const ofMesh& mesh, float normalLength) {
	for(int i = 0; i < mesh.getNumNormals(); i++) {
		const ofVec3f& start = mesh.getVertices()[i];
		const ofVec3f& normal = mesh.getNormals()[i];
		ofVec3f end = start + normal * normalLength;
		ofLine(start, end);
	}
}

ofMesh collapseModel(ofxAssimpModelLoader& model) {
	ofMesh mesh;
	for(int i = 0; i < model.getNumMeshes(); i++) {
		ofMesh curMesh = model.getMesh(i);
		mesh.append(curMesh);
	}
	return mesh;
}


void prepareRender(bool useDepthTesting, bool useBackFaceCulling, bool useFrontFaceCulling) {
	ofSetDepthTest(useDepthTesting);
	if(useBackFaceCulling || useFrontFaceCulling) {
		glEnable(GL_CULL_FACE);
		if(useBackFaceCulling && useFrontFaceCulling) {
			glCullFace(GL_FRONT_AND_BACK);
		} else if(useBackFaceCulling) {
			glCullFace(GL_BACK);
		} else if(useFrontFaceCulling) {
			glCullFace(GL_FRONT);
		}
	} else {
		glDisable(GL_CULL_FACE);
	}
}


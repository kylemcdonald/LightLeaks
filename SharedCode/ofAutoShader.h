#pragma once

class ofAutoShader : public ofShader {
public:
	void loadAuto(string name) {
		this->name = name;
		ofEventArgs args;
		update(args);
		ofAddListener(ofEvents().update, this, &ofAutoShader::update);
	}
    
	void update(ofEventArgs &args) {	
		bool needsReload = false;
			
		string fragName = name + ".frag";
		ofFile fragFile(fragName);
		if(fragFile.exists()) {
            time_t fragTimestamp = filesystem::last_write_time(fragFile);
			if(fragTimestamp != lastFragTimestamp) {
                cout << "[ofAutoShader] Timestamp for " << fragName << ": " << fragTimestamp << endl;
				needsReload = true;
				lastFragTimestamp = fragTimestamp;
			}
		} else {
			fragName = "";
		}
		
		string vertName = name + ".vert";
		ofFile vertFile(vertName);
        if(vertFile.exists()) {
            time_t vertTimestamp = filesystem::last_write_time(vertFile);
			if(vertTimestamp != lastVertTimestamp) {
                cout << "[ofAutoShader] Timestamp for " << vertName << ": " << vertTimestamp << endl;
				needsReload = true;
				lastVertTimestamp = vertTimestamp;
			}
		} else {
			vertName = "";
		}
		
		if(needsReload) {
            ofSleepMillis(100);
			ofShader::load(vertName, fragName);
            ready = checkReady();
		}
	}
    
    bool isReady() {
        return ready;
    }
protected:
    // originally checkProgramLinkStatus
    bool checkReady() {
        GLuint program = getProgram();
        GLint status;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        GLuint err = glGetError();
        if (err != GL_NO_ERROR){
            return false;
        }
        if (status == GL_FALSE) {
            return false;
        }
        return true;
    }
private:
	string name;
	time_t lastFragTimestamp, lastVertTimestamp;
    bool ready = false;
};

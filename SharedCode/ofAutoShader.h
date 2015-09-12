#pragma once

class ofAutoShader : public ofShader {
public:
	void setup(string name) {
		this->name = name;
		ofEventArgs args;
		update(args);
		ofAddListener(ofEvents().update, this, &ofAutoShader::update);
	}
	
	void update(ofEventArgs &args) {	
		bool needsReload = false;
			
		string fragName = name + ".fs";
		ofFile fragFile(fragName);
		if(fragFile.exists()) {
            time_t fragTimestamp = filesystem::last_write_time(fragFile);
			if(fragTimestamp != lastFragTimestamp) {
				needsReload = true;
				lastFragTimestamp = fragTimestamp;
			}
		} else {
			fragName = "";
		}
		
		string vertName = name + ".vs";
		ofFile vertFile(vertName);
        if(vertFile.exists()) {
            time_t vertTimestamp = filesystem::last_write_time(vertFile);
			if(vertTimestamp != lastVertTimestamp) {
				needsReload = true;
				lastVertTimestamp = vertTimestamp;
			}
		} else {
			vertName = "";
		}
		
		if(needsReload) {
			ofShader::load(vertName, fragName);
		}
	}
private:
	string name;
	time_t lastFragTimestamp, lastVertTimestamp;
};

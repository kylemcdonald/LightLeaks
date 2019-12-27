#pragma once

template <class T>
class ofAutoImage : public ofImage_<T> {
public:
	void loadAuto(string name) {
        setFilename(name);
		ofAddListener(ofEvents().update, this, &ofAutoImage::update);
	}
    
    void setFilename(string name) {
        this->name = name;
        ofEventArgs args;
        update(args);
    }
	
	void update(ofEventArgs &args) {
        reload();
	}
    
    void reload(bool force=false) {
        ofFile file(name);
        if(file.exists()) {
            time_t timestamp = filesystem::last_write_time(file);
            if(force || timestamp != lastTimestamp) {
                ofSleepMillis(100);
                if(!ofImage_<T>::load(name)) {
                    cout << "[ofAutoImage] Exists, but error loading: " << name << endl;
                } else {
                    cout << "[ofAutoImage] Timestamp for " << name << ": " << timestamp << endl;
                    lastTimestamp = timestamp;
                }
            }
        } else {
            cout << "[ofAutoImage] Does not exist: " << name << endl;
        }
    }
private:
    string name = "";
    time_t lastTimestamp = 0;
};

#pragma once

template <class T>
class ofAutoImage : public ofImage_<T> {
public:
	void loadAuto(string name) {
		this->name = name;
		ofEventArgs args;
		update(args);
		ofAddListener(ofEvents().update, this, &ofAutoImage::update);
	}
	
	void update(ofEventArgs &args) {
		ofFile file(name);
		if(file.exists()) {
            time_t timestamp = filesystem::last_write_time(file);
			if(timestamp != lastTimestamp) {
                cout << "[ofAutoImage] Timestamp for " << name << ": " << timestamp << endl;
                lastTimestamp = timestamp;
                if(!ofImage_<T>::load(name)) {
                    cout << "[ofAutoImage] Exists, but error loading: " << name << endl;
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

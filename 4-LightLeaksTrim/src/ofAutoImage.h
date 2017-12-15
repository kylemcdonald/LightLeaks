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
            cout << "timestamp for " << name << ": " << timestamp << endl;
			if(timestamp != lastTimestamp) {
                ofImage_<T>::load(name);
				lastTimestamp = timestamp;
			}
		}
	}
private:
    string name = "";
    time_t lastTimestamp = 0;
};

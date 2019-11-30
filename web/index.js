const express = require('express')
const fs = require('fs')
const livereload = require('livereload');
const fetch = require('node-fetch');
const reload = livereload.createServer();
reload.watch(__dirname + "/src/");
reload.watch(__dirname + "/dist/");
const path = require('path');
const moment = require('moment')

const app = express()
var expressWs = require('express-ws')(app);

let pattern = 0;
let numPatterns = 0;
let patternPath = ""
let folderName = ""

let photoState;
let cameraWs;

let fetchingImage = false;

const settingsPath = '../SharedData/settings.json';
let settings = JSON.parse(fs.readFileSync(settingsPath));
console.log(settings)


app.ws('/cam', function(ws, req) {
    index = 0;

    ws.on('message', async function(msg) {
        // console.log("Cam msg", msg)
        // ws.send(msg);

        if(Buffer.isBuffer(msg)) {
            fetchingImage = false
            if(photoState === 'preview') {
                if(controlWs && controlWs.readyState === 1){
                    controlWs.send(msg)
                    photoState = undefined;
                }
            } 
            else if(photoState === 'run') { 
                const fpath = patternPath +'.jpg';
                console.log(`(${pattern} / ${numPatterns}) Saving ${fpath}`);
                const folder = path.dirname(fpath);
                fs.mkdirSync(folder, {recursive: true});
                fs.writeFileSync(fpath, msg)

                onPhotoStored();
            }
        } else {
            console.log("Cam txt msg", msg)
            if(controlWs && controlWs.readyState === 1){
                controlWs.send(msg)
            }
        }
    });

    console.log ("Connection on /cam")
    for(const k of Object.keys(settings['camera'])){
        ws.send('setConfig:'+k+':'+settings['camera'][k])
        console.log('setConfig:'+k+':'+settings['camera'][k])
    }
    
    cameraWs = ws;
        
    // setTimeout(()=> cameraWs.send('photo'), 1000)
    
    ws.on('disconnect', () => {
        console.log("Disconnect")
        cameraWs = undefined;
    })
});

let controlWs;
app.ws('/control', (ws, req) => {   
    console.log("Connection on /control")

    ws.send('settings:' +JSON.stringify(settings));

    ws.on('message', async (msg) => {
        console.log('/control', msg)
        if(msg === 'preview') {
            if(cameraWs) {
                if(fetchingImage) {
                    console.error("Already fetching image")
                    return;
                }
                photoState = 'preview'
                cameraWs.send('preview')
                fetchingImage = true;
            }
        } else if(msg === 'start') {
            // if(cameraWs && cameraWs.readyState === 1) {
               startCalibration();
            // } else {
                // console.log("Camera not connected")
            // }
        } else if (msg.indexOf('setConfig:') == 0) {
            if(cameraWs) {
                const ar = msg.split(':')
                const replace = msg.replace('setConfig:'+ar[1]+":", '')

                if(ar[1] !== 'focus') {
                    console.log("Set "+ar[1])
                    settings = JSON.parse(fs.readFileSync(settingsPath));
                    settings['camera'][ar[1]] = replace
                    fs.writeFileSync(settingsPath, JSON.stringify(settings, null, 2));
                }
                cameraWs.send(msg);
            }
        }
    })
    controlWs = ws;
})

async function onPhotoStored() {
    if(pattern ++ < numPatterns - 1){
        await setPattern(pattern);
        if(fetchingImage) {
            console.error("Already fetching image")
            return;
        }
        fetchingImage = true;
        cameraWs.send('photo:'+patternPath)
    } else {
        cameraWs.send('scanComplete');
        controlWs.send('scanComplete');
        console.log("~~DONE~~")
    }
}

async function startCalibration() {
    console.log("\nStarting");
    pattern = 0;
    numPatterns = await fetch('http://localhost:8000/actions/numPatterns')
    .then( res => res.text())                
    .catch( e => console.error("Could not connect to openFrameworks"))

    folderName = 'scan-'+moment().format('MMDDTHH-mm-ss')
    console.log("Num patterns "+numPatterns);

    await setPattern(0);

    photoState = 'run'

    if(fetchingImage) {
        console.error("Already fetching image")
        return;
    }
    fetchingImage = true;
    cameraWs.send('photo:'+patternPath)
}
 
async function setPattern(pattern) {
    patternPath = await fetch('http://localhost:8000/actions/pattern/'+pattern).then( res => res.text());
    patternPath = '../SharedData/' + folderName + '/cameraImages/' + patternPath
    console.log("Pattern "+ pattern+ " "+patternPath)
}

app.use(express.static('src'))
app.use(express.static('dist'))

app.listen(9000, () => console.log('Listening on 9000'))


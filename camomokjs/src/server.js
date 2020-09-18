const express = require('express');
const path = require('path');
const fs = require('fs');

const app = express();

const shareDataPath = path.join(__dirname, '../../SharedData/')
app.use(express.static(path.join(__dirname, '../public')  , {
  etag: false
}));
app.use('/SharedData',express.static(shareDataPath, {
  etag: false
}));
app.use(express.json())
app.disable('etag');


async function getScans(){
  return new Promise((res)=>{
    const scans = [];
    fs.readdir(shareDataPath, (err, files)=>{
      for(const folder of files){
        if(fs.existsSync(path.join(shareDataPath,folder,"cameraImages" ))){
          scans.push(folder)
        }
      }
      res(scans);
    })
  })
}

app.get('/scans', async (req, res)=>{
 const scans = await getScans();
 res.send(scans)
})

app.post('/saveCalibration', async (req,res)=>{
  if(req.body.scan) {
    fs.writeFileSync(path.join(shareDataPath, req.body.scan, 'camamok.json'), JSON.stringify(req.body.data, null, 1))
  }
  res.send();
})

app.listen(8080, () => {
  console.log('Camamok started and available at http://localhost:8080');
});


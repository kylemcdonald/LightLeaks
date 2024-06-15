const express = require("express");
const path = require("path");
const fs = require("fs");
const bodyParser = require("body-parser");
const multer = require("multer");
const os = require("os");
const fetch = require("node-fetch");
const request = require("request");
const shell = require("shelljs");
const ExifReader = require("exifreader");
const fsPromise = require("fs/promises");

const app = express();

const shareDataPath =
  process.env.SHARED_DATA || path.join(__dirname, "../../SharedData/");

const scheduleJsonTmp = path.join(shareDataPath, "_scheduledDownload.json");

var download = function (uri, filename, callback) {
  request.head(uri, function (err, res, body) {
    if (!err) {
      console.log("content-type:", res.headers["content-type"]);
      console.log("content-length:", res.headers["content-length"]);

      request(uri).pipe(fs.createWriteStream(filename)).on("close", callback);
    } else {
      console.error(err);
      callback();
    }
  });
};

app.use(
  express.static(path.join(__dirname, "../public"), {
    // etag: false
  })
);
app.use(
  "/SharedData",
  express.static(shareDataPath, {
    // etag: false
  })
);
app.use(bodyParser({ limit: "100mb" }));

async function getScans() {
  return new Promise((res) => {
    const scans = [];
    fs.readdir(shareDataPath, (err, files) => {
      for (const folder of files) {
        if (fs.existsSync(path.join(shareDataPath, folder, "cameraImages"))) {
          scans.push(folder);
        }
      }
      res(scans);
    });
  });
}

app.get("/status/SharedData/:path*", async (req, res) => {
  res.send({
    exists: fs.existsSync(
      path.join(shareDataPath, req.path.replace("/status/SharedData", ""))
    ),
  });
});

app.get("/scans", async (req, res) => {
  const scans = await getScans();
  res.send(scans);
});

app.post("/saveCalibration", async (req, res) => {
  if (req.body.scan) {
    const dir = path.join(shareDataPath, req.body.scan, "camamok");
    if (!fs.existsSync(dir)) {
      fs.mkdirSync(dir);
    }
    fs.writeFileSync(
      path.join(dir, "camamok.json"),
      JSON.stringify(req.body.data, null, 1)
    );
  }
  res.send();
});

var upload = multer({ dest: os.tmpdir()});
var type = upload.single("image");

app.post("/saveXYZMap/:scan", upload.single("image"), async (req, res) => {
  const dir = path.join(shareDataPath, req.params.scan, "camamok");
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir);
  }
  
  fs.copyFileSync(req.file.path, path.join(dir, "xyzMap.raw"));
  fs.unlinkSync(req.file.path);

  res.send();
});

app.post(
  "/saveMask/:scan",
  upload.fields([
    { name: "image", maxCount: 1 },
    { name: "json", maxCount: 1 },
  ]),
  async (req, res) => {
    const dir = path.join(shareDataPath, req.params.scan, "camamok");
    if (!fs.existsSync(dir)) {
      fs.mkdirSync(dir);
    }
    fs.copyFileSync(req.files["image"][0].path, path.join(dir, "mask.jpg"));
    fs.unlinkSync(req.files["image"][0].path);

    fs.writeFileSync(path.join(dir, "mask.json"), req.body.json);

    res.send();
  }
);

app.post(
  "/saveProjectorMask",
  upload.fields([
    { name: "image", maxCount: 1 },
    { name: "json", maxCount: 1 },
  ]),
  async (req, res) => {
    const dir = path.join(shareDataPath);

    fs.copyFileSync(req.files["image"][0].path, path.join(dir, "mask-0.png"));
    fs.unlinkSync(req.files["image"][0].path);

    fs.writeFileSync(path.join(dir, "mask.json"), req.body.json);

    res.send();
  }
);

app.get("/downloadImageFromUrl", async (req, res) => {
  const dir = path.join(shareDataPath, path.dirname(req.query.filename));
  shell.mkdir("-p", dir);

  download(req.query.url, path.join(shareDataPath, req.query.filename), () => {
    res.send({});
  });
});

app.get("/scheduleDownloadImageFromSD", async (req, res) => {
  const dir = path.join(shareDataPath, path.dirname(req.query.filename));
  shell.mkdir("-p", dir);

  let scheduledDownload = [];
  try {
    const rawdata = fs.readFileSync(scheduleJsonTmp);
    scheduledDownload = JSON.parse(rawdata);
  } catch (e) {}

  scheduledDownload.push({ url: req.query.url, dest: req.query.filename });
  fs.writeFileSync(scheduleJsonTmp, JSON.stringify(scheduledDownload));
  res.send({});
});

app.get("/getExifFromUrl", async (req, res) => {
  const dir = "/tmp";
  download(req.query.url, path.join(dir, req.query.filename), async () => {
    const tags = await ExifReader.load(path.join(dir, req.query.filename));
    console.log(tags);
    res.send(tags);
  });
});

app.get("/proxy", async (req, res, next) => {
  fetch(req.query.url)
    .then((blob) => blob.text())
    .then((resp) => res.send(resp))
    .catch(next)
});

app.post("/proxy/", async (req, res, next) => {
  try {
    console.log(req.body);
    fetch(req.query.url, {
      method: "post",
      body: JSON.stringify(req.body),
      headers: {
        "Content-Type": "application/json",
      },
    })
      .then((blob) => blob.json())
      .then((json) => res.send(json))
    .catch(next);
    // console.log(apiResponse)
  } catch (e) {
    console.warn(e);
  }
});

app.put("/proxy/", async (req, res, next) => {
  try {
    console.log(req.body);
    fetch(req.query.url, {
      method: "put",
      body: JSON.stringify(req.body),
      headers: {
        "Content-Type": "application/json",
      },
    })
      .then((blob) => blob.json())
      .then((json) => res.send(json))
      .catch(next);
    // console.log(apiResponse)
  } catch (e) {
    console.warn(e);
  }
});

app.listen(8080, () => {
  console.log("Camamok started and available at http://localhost:8080");
});

let checkSdInProgress = false;
async function checkSD() {
  if (checkSdInProgress) return;
  checkSdInProgress = true;
  const exists = fs.existsSync("/Volumes/EOS_DIGITAL/");
  if (!exists) return;
  if (!fs.existsSync(scheduleJsonTmp)) return;

  const rawdata = fs.readFileSync(scheduleJsonTmp);
  scheduledDownload = JSON.parse(rawdata);

  if (scheduledDownload.length == 0) return;

  console.log("SD Card found");

  const promises = [];
  for ({ dest, url } of scheduledDownload) {
    const sdpath = url.replace(/.+sd\/(.+)/gim, "/Volumes/EOS_DIGITAL/DCIM/$1");
    console.log(`Downloading ${sdpath} to ${path.join(shareDataPath, dest)}`);
    promises.push(fsPromise.copyFile(sdpath, path.join(shareDataPath, dest)));
  }

  await Promise.all(promises);
  console.log(`DONE, downloaded ${promises.length} images`);
  fs.writeFileSync(scheduleJsonTmp, "[]");

  checkSdInProgress = false;
}

setInterval(checkSD, 5000);

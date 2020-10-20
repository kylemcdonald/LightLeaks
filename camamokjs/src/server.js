const express = require("express");
const path = require("path");
const fs = require("fs");
const bodyParser = require("body-parser");
const multer = require("multer");
const os = require("os");

const app = express();

const shareDataPath =
  process.env.SHARED_DATA || path.join(__dirname, "../../SharedData/");
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

var upload = multer({ dest: os.tmpdir() });
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

app.listen(8080, () => {
  console.log("Camamok started and available at http://localhost:8080");
});

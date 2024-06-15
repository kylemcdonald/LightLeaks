/**
 * Download scans from the SD card
 */
const data = require("../../SharedData/_scheduledDownload.json");
const fs = require("fs");
const sdPath = "/Volumes/LEXAR/DCIM/";

console.log(`Transfering ${data.length} files...`);

for (const file of data) {
  const source = `${sdPath}${file.url.split("/sd/")[1]}`;
  const dst = `../SharedData/${file.dest}`;

  console.log(`Transfering ${source} to ${dst}`);

  fs.mkdirSync(dst.split("/").slice(0, -1).join("/"), { recursive: true });
  fs.copyFileSync(source, dst);
}

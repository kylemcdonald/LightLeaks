const express = require("express");
const path = require("path");
const fs = require("fs");
const bodyParser = require("body-parser");
const multer = require("multer");
const os = require("os");
const fetch = require("node-fetch");
const request = require('request');
const shell = require('shelljs');
const ExifReader = require('exifreader');
const moment = require('moment')

const shareDataPath =
  process.env.SHARED_DATA || path.join(__dirname, "../../SharedData/");

async function run(){
  // for(let i=1647;i<1651;i++){
  for(let i=1647;i<1748;i++){
    try {
    const tags1 = await ExifReader.load(path.join(shareDataPath, `test/IMG_${i}.JPG`));
    const tags2 = await ExifReader.load(path.join(shareDataPath, `test/IMG_${i+1}.JPG`));

    const time1 = tags1.DateTime.description + "." + tags1.SubSecTime.description
    const time2 = tags2.DateTime.description + "." + tags2.SubSecTime.description

    const diff = moment(time2, "YYYY:MM:DD hh:mm:ss.SS").diff(
      moment(time1, "YYYY:MM:DD hh:mm:ss.SS")
    );

    console.log(diff)
    } catch(e){

    }
  }
}

run();
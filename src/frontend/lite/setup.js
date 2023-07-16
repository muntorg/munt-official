const fs = require("fs");
const path = require("path");
let src = path.join(__dirname, `holdinAPI.js.in`);
let dst = path.join(__dirname, `holdinAPI.js`);
fs.copyFile(src, dst, fs.constants.COPYFILE_EXCL, err => {});

let src_filter = path.join(__dirname, `../../data/staticfiltercp`);
let dst_filter = path.join(__dirname, `/public/staticfiltercp`);
fs.copyFile(src_filter, dst_filter, fs.constants.COPYFILE_EXCL, err => {});

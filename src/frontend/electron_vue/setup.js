const fs = require("fs");
const path = require("path");
let src = path.join(__dirname, `holdinAPI.js.in`);
let dst = path.join(__dirname, `holdinAPI.js`);
fs.copyFile(src, dst, fs.constants.COPYFILE_EXCL, err => {
  console.warn(err);
});

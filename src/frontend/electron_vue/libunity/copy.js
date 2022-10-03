var os = require("os");

let repo =
  "https://github.com/muntorg/munt-official/releases/download/development/";

let file = "";
if (os.platform() === "win32") {
  file = "libmunt_win_" + os.arch() + ".node";
} else if (os.platform() === "linux") {
  file = "libmunt_linux_" + os.arch() + ".node";
} else if (os.platform() === "darwin") {
    file = "libmunt_macos_" + os.arch() + ".node";
} else {
  throw "Unable to determine platform";
}

const fs = require("fs");
const request = require("request");
let error;
const download = (uri, filename) => {
  request(uri)
    .on("error", err => {
      error = err;
    })
    .on("response", response => {
      if (response.statusCode !== 200) {
        error = `error downloading ${uri} : ${response.statusCode}`;
      } else {
        console.log(`downloading ${uri}...`);
      }
    })
    .pipe(fs.createWriteStream(filename))
    .on("close", () => {
      if (error !== undefined) console.error(error);
      else console.log(`fetched ${src} -> ${dst}`);
    });
};

const path = require("path");
let src = repo + file;
let dst = path.join(__dirname, `../src/unity/lib_unity.node`);
download(src, dst);

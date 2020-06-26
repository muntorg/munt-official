var os = require("os");

let repo =
  "https://github.com/novocurrency/novocurrency-core/releases/latest/download/";

let file = "";
if (os.platform() === "win32") {
  file = "libnovo_win_" + os.arch() + ".node";
} else if (os.platform() === "linux") {
  file = "libnovo_linux_" + os.arch() + ".node";
} else if (os.platform() === "darwin") {
  file = "libnovo_macos_" + os.arch() + ".node";
} else {
  throw "Unable to determine platform";
}

const fs = require("fs");
const request = require("request");
const download = (uri, filename, callback) => {
  request.head(uri, (err, res) => {
    if (err) {
      return console.error(err);
    }
    if (res.statusCode !== 200) {
      return console.error(`Error downloading ${filename}: ${res.statusCode}`);
    }

    request(uri)
      .pipe(fs.createWriteStream(filename))
      .on("close", callback);
  });
};

const path = require("path");
let src = repo + file;
let dst = path.join(__dirname, `../src/unity/lib_unity.node`);
download(src, dst, () => {
  console.log(`fetched ${src} -> ${dst}`);
});

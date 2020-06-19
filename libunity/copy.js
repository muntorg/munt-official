const path = require("path");
const fs = require("fs");

let dir = path.join(__dirname, `${process.platform}/`);
let files = fs.readdirSync(dir);
let file = files.find(x => x.indexOf('lib_unity_node_js') === 0);
let src = path.join(__dirname, `${process.platform}/${file}`);
let dst = path.join(__dirname, `../src/unity/lib_unity.node`);

fs.copyFile(src, dst, (err) => {
    if (err) throw err;
    console.log(`copied ${src} -> ${dst}`);
});

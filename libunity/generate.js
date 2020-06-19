var fs = require("fs");
var path = require("path");

const skipFunctions = [
  "InitUnityLib",
  "InitUnityLibThreaded",
  "InitWalletFromAndroidLegacyProtoWallet",
  "InitWalletLinkedFromURI",
  "isValidAndroidLegacyProtoWallet"
];

let inputFile = path.join(__dirname, "unifiedbackend_doc.js");
let backendFile = path.join(
  __dirname,
  "/../src/unity/UnityBackend.js"
);
let libFile = path.join(__dirname, "/../src/unity/LibUnity.js");

let dataToParse = fs.readFileSync(inputFile, "utf8").split("\n");

let allFunctions = [];
let comments = [];
let startFound = false;
let endFound = false;

while (endFound === false) {
  let line = dataToParse.shift().trim();
  if (startFound === false) {
    if (line.indexOf("class NJSUnifiedBackend") !== -1) {
      startFound = true;
    }
    continue;
  }

  if (line === "}") {
    endFound = true;
    continue;
  }

  line = line.replace("\r\n", "");

  if (line.startsWith("static declare function") === false) {
    if (line.startsWith("/")) {
      comments.push(line);
    }
    if (line.startsWith("*")) {
      comments.push(` ${line}`);
    }
    continue;
  }

  line = line.replace("static declare function", "").trim();

  let functionOpeningBracketIdx = line.indexOf("(");
  let functionClosingBracketIdx = line.indexOf(")");
  let name = line.substr(0, functionOpeningBracketIdx);

  if (skipFunctions.find(x => x === name)) {
    comments = [];
    continue;
  }

  let args = line.substr(
    functionOpeningBracketIdx + 1,
    functionClosingBracketIdx - functionOpeningBracketIdx - 1
  );

  if (args.length > 0) {
    let argsSplit = args.split(", ");
    let arr = [];
    argsSplit.forEach(arg => {
      arr.push(arg.substr(0, arg.indexOf(":")));
    });

    args = arr.join(", ");
  }
  allFunctions.push({
    functionName: name.charAt(0).toUpperCase() + name.slice(1),
    name,
    args,
    comments: comments
  });
  comments = [];
}

allFunctions.sort((a, b) => {
  let A = a.functionName.toUpperCase();
  let B = b.functionName.toUpperCase();

  if (A < B) {
    return -1;
  }
  if (A > B) {
    return 1;
  }
  // names must be equal
  return 0;
});

let txtBackend = ``;
let txtLib = ``;
let tabs = 1;
for (let i = 0; i < allFunctions.length; i++) {
  let f = allFunctions[i];

  for (var j = 0; j < f.comments.length; j++) {
    txtBackend = addLine(txtBackend, f.comments[j], tabs);
  }

  txtBackend = addLine(
    txtBackend,
    `static ${f.functionName}(${f.args}) {`,
    tabs
  );
  txtBackend = addLine(
    txtBackend,
    `return ipc.sendSync("${f.functionName}"${
      f.args.length > 0 ? ", " + f.args : ""
    });`,
    tabs + 1
  );
  txtBackend = addLine(txtBackend, "}", tabs, 2);

  txtBackend = addLine(
    txtBackend,
    `static ${f.functionName}Async(${f.args}) {`,
    tabs
  );
  txtBackend = addLine(
    txtBackend,
    `return ipc.callMain("${f.functionName}"${
      f.args.length > 0 ? ", { " + f.args + " }" : ""
    });`,
    tabs + 1
  );
  txtBackend = addLine(txtBackend, "}", tabs, 2);

  tabs = 2;
  txtLib = addLine(
    txtLib,
    `ipc.on("${f.functionName}", ${
      f.args.length > 0 ? "(event, " + f.args + ")" : "event"
    } => {`,
    tabs
  );
  txtLib = addLine(
    txtLib,
    `let result = this.backend.${f.name}(${f.args});`,
    tabs + 1
  );
  txtLib = addLine(
    txtLib,
    `this._handleIpcInternal("${f.functionName}", result);`,
    tabs + 1
  );
  txtLib = addLine(txtLib, `event.returnValue = result;`, tabs + 1);
  txtLib = addLine(txtLib, `});`, tabs, 2);

  txtLib = addLine(
    txtLib,
    `ipc.answerRenderer("${f.functionName}", async ${
      f.args.length > 0 ? "data" : "()"
    } => {`,
    tabs
  );
  txtLib = addLine(
    txtLib,
    `let result = this.backend.${f.name}(${
      f.args.length > 0 ? "data." + f.args.split(", ").join(", data.") : ""
    });`,
    tabs + 1
  );
  txtLib = addLine(
    txtLib,
    `this._handleIpcInternal("${f.functionName}", result);`,
    tabs + 1
  );
  txtLib = addLine(txtLib, `return result;`, tabs + 1);
  txtLib = addLine(
    txtLib,
    `});`,
    tabs,
    i == allFunctions.length - 1 ? 1 : 2
  );
}

let replace = "";
replace = addLine(replace, "/* inject:code */");
replace += txtBackend;
replace = addLine(replace, "/* inject:code */", 1, 0);
let backendData = fs.readFileSync(backendFile, "utf-8");
var result = backendData.replace(
  /\/\* inject:code \*\/(.)+\/\* inject:code \*\//s,
  replace
);

fs.writeFileSync(backendFile, result, "utf8");

replace = "";
replace = addLine(replace, "/* inject:code */");
replace += txtLib;
replace = addLine(replace, "/* inject:code */", 1, 0);

let libData = fs.readFileSync(libFile, "utf-8");
result = libData.replace(
  /\/\* inject:code \*\/(.)+\/\* inject:code \*\//s,
  replace
);
fs.writeFileSync(libFile, result, "utf8");

function addLine(txt, line, tabs = 0, rns = 1) {
  txt += `${" ".repeat(tabs * 2)}${line}${"\r\n".repeat(rns)}`;
  return txt;
}

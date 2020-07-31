var fs = require("fs");
var path = require("path");

let inputFile = path.join(
  __dirname,
  "../../../unity/djinni/node_js/unifiedbackend_doc.js"
);

const generateInfo = [
  {
    className: "NJSILibraryController",
    controller: "libraryController",
    functions: [
      "ChangePassword",
      "GenerateRecoveryMnemonic",
      "GetMnemonicDictionary",
      "GetRecoveryPhrase",
      "GetTransactionHistory",
      "InitWalletFromRecoveryPhrase",
      "IsValidNativeAddress",
      "IsValidRecipient",
      "IsValidRecoveryPhrase",
      "LockWallet",
      "PerformPaymentToRecipient",
      "ResendTransaction",
      "UnlockWallet"
    ],
    results: []
  },
  {
    className: "NJSIAccountsController",
    controller: "accountsController",
    functions: ["CreateAccount", "GetActiveAccountBalance", "SetActiveAccount"],
    results: []
  },
  {
    className: "NJSIGenerationController",
    controller: "generationController",
    functions: ["StartGeneration", "StopGeneration"],
    results: []
  }
];

let backendFile = path.join(__dirname, "/../src/unity/UnityBackend.js");
let libFile = path.join(__dirname, "/../src/unity/LibUnity.js");

let inputFileData = fs.readFileSync(inputFile, "utf8");

let txtUnityBackend = "";
let txtLibUnity = "";

for (var i = 0; i < generateInfo.length; i++) {
  let o = generateInfo[i];
  generateCodeFor(o);

  for (var j = 0; j < o.results.length; j++) {
    let f = o.results[j];

    let tabs = 1;

    // ipc.sendSync <- Sync
    txtUnityBackend = addLine(
      txtUnityBackend,
      `static ${f.functionName}(${f.args}) {`,
      tabs
    );
    txtUnityBackend = addLine(
      txtUnityBackend,
      `return ipc.sendSync("${f.functionName}"${
        f.args.length > 0 ? ", " + f.args : ""
      });`,
      tabs + 1
    );
    txtUnityBackend = addLine(txtUnityBackend, "}", tabs, 2);

    // ipc.callMain <- Async
    txtUnityBackend = addLine(
      txtUnityBackend,
      `static ${f.functionName}Async(${f.args}) {`,
      tabs
    );
    txtUnityBackend = addLine(
      txtUnityBackend,
      `return ipc.callMain("${f.functionName}"${
        f.args.length > 0 ? ", { " + f.args + " }" : ""
      });`,
      tabs + 1
    );
    txtUnityBackend = addLine(txtUnityBackend, "}", tabs, 2);

    // ipc.on -> Sync
    tabs = 2;
    txtLibUnity = addLine(
      txtLibUnity,
      `ipc.on("${f.functionName}", ${
        f.args.length > 0 ? "(event, " + f.args + ")" : "event"
      } => {`,
      tabs
    );
    txtLibUnity = addLine(
      txtLibUnity,
      `let result = this._preExecuteIpcCommand("${f.functionName}");`,
      tabs + 1
    );
    txtLibUnity = addLine(txtLibUnity, `if (result === undefined) {`, tabs + 1);
    txtLibUnity = addLine(
      txtLibUnity,
      `result = this.${o.controller}.${f.name}(${f.args});`,
      tabs + 2
    );
    txtLibUnity = addLine(txtLibUnity, `}`, tabs + 1);
    txtLibUnity = addLine(
      txtLibUnity,
      `this._postExecuteIpcCommand("${f.functionName}", result);`,
      tabs + 1
    );
    txtLibUnity = addLine(txtLibUnity, `event.returnValue = result;`, tabs + 1);
    txtLibUnity = addLine(txtLibUnity, `});`, tabs, 2);

    // ipc.answerRenderer -> Async
    txtLibUnity = addLine(
      txtLibUnity,
      `ipc.answerRenderer("${f.functionName}", async ${
        f.args.length > 0 ? "data" : "()"
      } => {`,
      tabs
    );
    txtLibUnity = addLine(
      txtLibUnity,
      `let result = this._preExecuteIpcCommand("${f.functionName}");`,
      tabs + 1
    );
    txtLibUnity = addLine(txtLibUnity, `if (result === undefined) {`, tabs + 1);
    txtLibUnity = addLine(
      txtLibUnity,
      `result = this.${o.controller}.${f.name}(${
        f.args.length > 0 ? "data." + f.args.split(", ").join(", data.") : ""
      });`,
      tabs + 2
    );
    txtLibUnity = addLine(txtLibUnity, `}`, tabs + 1);
    txtLibUnity = addLine(
      txtLibUnity,
      `this._postExecuteIpcCommand("${f.functionName}", result);`,
      tabs + 1
    );
    txtLibUnity = addLine(txtLibUnity, `return result;`, tabs + 1);
    txtLibUnity = addLine(
      txtLibUnity,
      `});`,
      tabs,
      j == o.results.length - 1 ? 1 : 2
    );
  }
}

function generateCodeFor(o) {
  let startFound = false;
  let endFound = false;
  let dataToParse = inputFileData.split("\n");

  while (endFound === false) {
    let line = dataToParse.shift().trim();
    if (startFound === false) {
      if (line.indexOf(`class ${o.className}`) !== -1) {
        startFound = true;
      }
      continue;
    }
    if (line === "}") {
      endFound = true;
      continue;
    }

    line = line.replace("\r\n", "");
    if (line.startsWith("static declare function") === false) continue;
    line = line.replace("static declare function", "").trim();

    let functionOpeningBracketIdx = line.indexOf("(");
    let functionClosingBracketIdx = line.indexOf(")");
    let name = line.substr(0, functionOpeningBracketIdx);

    let exists = false;
    for (var i = 0; i < o.functions.length; i++) {
      if (name.toLowerCase() === o.functions[i].toLowerCase()) {
        exists = true;
        break;
      }
    }

    if (exists === false) continue;

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

    o.results.push({
      functionName: name.charAt(0).toUpperCase() + name.slice(1),
      name,
      args
    });
  }
}

let replace = "";
replace = addLine(replace, "/* inject:code */");
replace += txtUnityBackend;
replace = addLine(replace, "/* inject:code */", 1, 0);
let backendData = fs.readFileSync(backendFile, "utf-8");
var result = backendData.replace(
  /\/\* inject:code \*\/(.)+\/\* inject:code \*\//s,
  replace
);

fs.writeFileSync(backendFile, result, "utf8");

replace = "";
replace = addLine(replace, "/* inject:code */");
replace += txtLibUnity;
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

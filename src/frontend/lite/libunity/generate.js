var fs = require("fs");
var path = require("path");

let inputFile = path.join(
  __dirname,
  "../../../unity/djinni/node_js/unifiedbackend_doc.js"
);

let controllerFile = path.join(__dirname, "/../src/unity/Controllers.js");
let libUnityFile = path.join(__dirname, "/../src/unity/LibUnity.js");

let fileToParse = fs.readFileSync(inputFile, "utf8");
let dataToParse = fileToParse.split("\n");

let controllers = [
  {
    className: "NJSILibraryController",
    exclude: [
      "InitUnityLib",
      "InitUnityLibThreaded",
      "ContinueWalletFromRecoveryPhrase",
      "InitWalletLinkedFromURI",
      "ContinueWalletLinkedFromURI",
      "InitWalletFromAndroidLegacyProtoWallet",
      "isValidAndroidLegacyProtoWallet",
      "PersistAndPruneForSPV",
      "getMutationHistory",
      "getTransactionHistory",
      "HaveUnconfirmedFunds",
      "GetBalance"
    ]
  },
  {
    className: "NJSIWalletController",
    exclude: ["setListener"]
  },
  {
    className: "NJSIRpcController",
    exclude: ["execute"],
    custom: [
      {
        name: "Execute",
        args: ["command"]
      }
    ]
  },
  {
    className: "NJSIP2pNetworkController",
    exclude: ["setListener"]
  },
  {
    className: "NJSIAccountsController",
    exclude: ["setListener"]
  }
];

for (let i = 0; i < controllers.length; i++) {
  getFunctionsFor(controllers[i]);
}

function getFunctionsFor(controller) {
  controller.functions = [];
  let exclude = controller.exclude || [];

  let startFound = false;
  let endFound = false;

  let i = 0;
  while (endFound === false) {
    let line = dataToParse[i++].trim();
    if (startFound === false) {
      if (line.indexOf(`declare class ${controller.className}`) !== -1) {
        startFound = true;
      }
      continue;
    } else if (line === "}") {
      endFound = true;
      continue;
    }

    line = line.replace("\r\n", "");
    if (line.startsWith("static declare function") === false) continue;
    line = line.replace("static declare function", "").trim();

    let openingBracketIdx = line.indexOf("(");
    let closingBracketIdx = line.indexOf(")");
    let name = line.substr(0, openingBracketIdx);

    let skip = false;
    for (let j = 0; j < exclude.length; j++) {
      if (name.toLowerCase() === exclude[j].toLowerCase()) {
        skip = true;
        break;
      }
    }
    if (skip) continue;

    let args = line.substr(
      openingBracketIdx + 1,
      closingBracketIdx - openingBracketIdx - 1
    );

    if (args.length > 0) {
      let argsSplit = args.split(", ");
      let arr = [];
      argsSplit.forEach(arg => {
        arr.push(arg.substr(0, arg.indexOf(":")));
      });

      args = arr.join(", ");
    }

    controller.functions.push({ name: name, args: args });
  }
}

function getControllerCode() {
  let code = [];

  for (let i = 0; i < controllers.length; i++) {
    let controller = controllers[i];
    let custom = controller.custom || [];

    code.push(`class ${controller.className.replace("NJSI", "")} {`);

    for (let j = 0; j < custom.length; j++) {
      let f = custom[j];
      if (j > 0) code.push(``);
      code.push(`static async ${f.name}Async(${f.args}) {`);
      code.push(
        `return handleError(await ipc.callMain("${controller.className}.${
          f.name
        }Async"${f.args.length > 0 ? ", {" + f.args + "}" : ""})
        );`
      );
      code.push(`}`);
      code.push(``);
      code.push(`static ${f.name}(${f.args}) {`);
      code.push(
        `return handleError(ipc.sendSync("${controller.className}.${f.name}"${
          f.args.length > 0 ? ", " + f.args : ""
        })
        );`
      );
      code.push(`}`);
    }

    for (let j = 0; j < controller.functions.length; j++) {
      let f = controller.functions[j];
      if (j > 0) code.push(``);
      code.push(`static async ${PascalCase(f.name)}Async(${f.args}) {`);
      code.push(
        `return handleError(await ipc.callMain("${controller.className}.${
          f.name
        }Async"${f.args.length > 0 ? ", {" + f.args + "}" : ""}));`
      );
      code.push(`}`);
      code.push(``);
      code.push(`static ${PascalCase(f.name)}(${f.args}) {`);
      code.push(
        `return handleError(ipc.sendSync("${controller.className}.${f.name}"${
          f.args.length > 0 ? ", " + f.args : ""
        }));`
      );
      code.push(`}`);
    }

    code.push(`}`);
    code.push(``);
  }

  code.push(`export {`);

  for (let i = 0; i < controllers.length; i++) {
    code.push(
      `${controllers[i].className.replace("NJSI", "")}${
        i + 1 < controllers.length ? "," : ""
      }`
    );
  }

  code.push(`}`);
  code.push(``);

  return code;
}

function getLibUnityCode() {
  let code = [];

  for (let i = 0; i < controllers.length; i++) {
    let controller = controllers[i];
    let className = controller.className.replace("NJSI", "");
    className = className.charAt(0).toLowerCase() + className.substr(1);

    if (i > 0) code.push(``);
    code.push(`// Register ${controller.className} ipc handlers`);

    for (let j = 0; j < controller.functions.length; j++) {
      let f = controller.functions[j];
      if (j > 0) code.push(``);
      code.push(
        `ipc.answerRenderer("${controller.className}.${f.name}Async", async ${
          f.args.length > 0 ? "data" : "()"
        } => {`
      );

      let args = f.args.length > 0 ? f.args.split(", ") : [];
      let consoleArgs = "";
      if (args.length > 0) {
        for (let k = 0; k < args.length; k++) {
          consoleArgs += k > 0 ? ", " : "";
          consoleArgs += "${data." + args[k] + "}";
        }
      }

      code.push(`
        console.log(\`IPC: ${className}.${f.name}Async(${consoleArgs})\`);
        try {
          let result = this.${className}.${f.name}(
            ${
              f.args.length > 0
                ? "data." + f.args.split(", ").join(", data.")
                : ""
            }
          );
          return {
            success: true,
            result: result
          };
        }
        catch(e) {
          return handleError(e);
        }
      `);
      code.push(`});`);
      code.push(``);
      code.push(
        `ipc.on("${controller.className}.${f.name}", ${
          f.args.length > 0 ? "(event, " + f.args + ")" : "event"
        } => {`
      );

      args = f.args.length > 0 ? f.args.split(", ") : [];
      consoleArgs = "";
      if (args.length > 0) {
        for (let k = 0; k < args.length; k++) {
          consoleArgs += k > 0 ? ", " : "";
          consoleArgs += "${" + args[k] + "}";
        }
      }

      code.push(`
        console.log(\`IPC: ${className}.${f.name}(${consoleArgs})\`);
        try {
          let result = this.${className}.${f.name}(${f.args});
          event.returnValue = {
            success: true,
            result: result
          };
        }
        catch(e) {
          event.returnValue = handleError(e);
        }
      `);
      code.push(`});`);
    }
  }

  code.push(``);
  return code;
}

let controllerData = fs.readFileSync(controllerFile, "utf-8");

let replace = "";
replace += "/* inject:generated-code */\r\n";
replace += getControllerCode().join("\r\n");
replace += "/* inject:generated-code */";

let result = controllerData.replace(
  /\/\* inject:generated-code \*\/(.)+\/\* inject:generated-code \*\//s,
  replace
);
fs.writeFileSync(controllerFile, result, "utf-8");

let libUnityData = fs.readFileSync(libUnityFile, "utf-8");
replace = "";
replace += "/* inject:generated-code */\r\n";
replace += getLibUnityCode().join("\r\n");
replace += "/* inject:generated-code */";

result = libUnityData.replace(
  /\/\* inject:generated-code \*\/(.)+\/\* inject:generated-code \*\//s,
  replace
);
fs.writeFileSync(libUnityFile, result, "utf-8");

function PascalCase(line) {
  return line.charAt(0).toUpperCase() + line.substr(1);
}

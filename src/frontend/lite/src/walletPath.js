import { app } from "electron";

const isDevelopment = process.env.NODE_ENV !== "production";
import os from "os";
import path from "path";

let walletPath = "";

if (process.type !== "renderer") {
  if (os.platform() === "linux") {
    walletPath = path.join(app.getPath("home"), isDevelopment ? ".munt_lite_dev" : ".munt_lite");
  } else {
    walletPath = app.getPath("userData");
    if (isDevelopment) walletPath = walletPath + "_dev";
  }
}

export default walletPath;

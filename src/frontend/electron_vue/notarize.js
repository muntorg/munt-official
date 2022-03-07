"use strict";

const path = require("path");
const { notarize } = require("electron-notarize");

const getAuthInfo = () => {
  const { APPLE_ID: appleId, APPLE_ID_PASSWORD: appleIdPassword, APP_ID: appId } = process.env;

  if (!appleId || !appleIdPassword) {
    throw new Error("One of APPLE_ID and APPLE_ID_PASSWORD environment variables is missing for notarization.");
  }
  if (!appId) {
    throw new Error("Unable to determine appID");
  }

  return {
    appleId,
    appleIdPassword
  };
};

module.exports = async params => {
  if (params.electronPlatformName !== "darwin") {
    return;
  }

  // Read and validate auth information from environment variables
  let authInfo;
  try {
    authInfo = getAuthInfo();
  } catch (error) {
    console.log(`Skipping notarization: ${error.message}`);
    return;
  }

  let appId = authInfo.appId;
  const appPath = path.join(params.appOutDir, `${params.packager.appInfo.productFilename}.app`);

  const notarizeOptions = { appBundleId: appId, appPath };
  notarizeOptions.appleId = authInfo.appleId;
  notarizeOptions.appleIdPassword = authInfo.appleIdPassword;

  console.log(`Notarizing ${appId} found at ${appPath}`);
  await notarize(notarizeOptions);
  console.log(`Done notarizing ${appId}`);
};

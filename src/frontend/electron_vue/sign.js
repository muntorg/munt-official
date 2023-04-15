exports.default = async function (configuration) {
  const CERTIFICATE_PATH = process.env.CERTIFICATE_PATH;
  const CERTIFICATE_KEY = process.env.CERTIFICATE_KEY;
  const CERTIFICATE_NAME = process.env.CERTIFICATE_NAME;
  const CERTIFICATE_URL = process.env.CERTIFICATE_URL;
  const CERTIFICATE_PASS = process.env.CERTIFICATE_PASS;
  require("child_process").execSync(
    `osslsigncode-2.0/osslsigncode -pass "${CERTIFICATE_PASS}" -spc "${CERTIFICATE_PATH}" -key "${CERTIFICATE_KEY}" -n "${CERTIFICATE_NAME}" -i "${CERTIFICATE_URL}" -in "${configuration.path}" -out "${configuration.path}2" && mv "${configuration.path}2" "${configuration.path}"`,
    {
      stdio: "inherit"
    }
  );
};

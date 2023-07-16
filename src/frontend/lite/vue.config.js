module.exports = {
  configureWebpack: {
    // Configuration applied to all builds
  },
  pluginOptions: {
    electronBuilder: {
      nodeIntegration: true,
      mainProcessWatch: [],
      mainProcessArgs: [],
      builderOptions: {
        appId: "org.munt.lite-desktop-wallet",
        productName: "Munt-lite",
        extraFiles: [],
        publish: null,
        afterSign: "./notarize.js",
        protocols: [
          {
            name: "muntlite",
            role: "Viewer",
            schemes: ["muntlite", "munt"]
          }
        ],
        mac: {
          category: "public.app-category.finance",
          asar: false,
          hardenedRuntime: true
        },
        win: {
          sign: "./sign.js"
        },
        linux: {
          category: "public.app-category.finance"
        },
        dmg: {
          background: "./build/background.tiff",
          contents: [
            {
              x: 410,
              y: 190,
              type: "link",
              path: "/Applications"
            },
            {
              x: 130,
              y: 190,
              type: "file"
            }
          ]
        }
      },
      // HACK! Work around issues with electron-vue-builder being too old and new webpack... (files not loading without this)
      customFileProtocol: "./"
    },
    i18n: {
      locale: "en",
      fallbackLocale: "en",
      localeDir: "locales",
      enableInSFC: false
    }
  },
  css: {
    loaderOptions: {
      less: {},
      postcss: { postcssOptions: { config: false } }
    }
  }
};

// HACK: OpenSSL 3 does not support md4 any more, but webpack hardcodes it all over the place: https://github.com/webpack/webpack/issues/13572
const crypto = require("crypto");
const crypto_orig_createHash = crypto.createHash;
crypto.createHash = algorithm => crypto_orig_createHash(algorithm == "md4" ? "sha256" : algorithm);

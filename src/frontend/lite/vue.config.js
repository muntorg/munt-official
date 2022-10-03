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
      }
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
      less: {
        javascriptEnabled: true
      }
    }
  }
};

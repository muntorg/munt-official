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
        appId: "com.novocurrency.app",
        productName: "Novo",
        extraFiles: [],
        publish: null
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

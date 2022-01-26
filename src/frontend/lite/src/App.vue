<template>
  <div id="app">
    <app-wallet v-if="isWallet" />
    <router-view v-else />
  </div>
</template>

<script>
import {mapState} from "vuex";
import AppWallet from "./AppWallet";

export default {
  name: "App",
  components: {
    AppWallet
  },
  created() {
    this.changeLanguage(this.language);
    document.querySelector("html").dataset.theme = this.theme;
  },
  watch: {
    language() {
      this.changeLanguage(this.language);
    },
    theme() {
      document.querySelector("html").dataset.theme = this.theme;
    }
  },
  computed: {
    ...mapState("app", ["language", "theme"]),
    isWallet() {
      switch (window.location.hash.toLowerCase()) {
        case "#/debug":
          return false;
        default:
          return true;
      }
    }
  },
  methods: {
    changeLanguage(language) {
      if (this.$i18n.locale === language) return;
      this.$i18n.locale = language;
      this.$forceUpdate();
    }
  }
};
</script>

<style lang="less">
@import "./css/app.less";

*,
#app {
  font-family: euclid, Helvetica, sans-serif;
  font-style: normal;
  font-weight: 400;
  font-size: 15px;
  -webkit-text-size-adjust: none;
  text-rendering: optimizeLegibility;
  font-variant-ligatures: none;
  -webkit-font-smoothing: antialiased;
}
</style>

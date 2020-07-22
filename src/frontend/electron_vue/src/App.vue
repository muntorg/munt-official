<template>
  <div id="app">
    <app-wallet v-if="isWallet" />
    <router-view v-else />
  </div>
</template>

<script>
import AppWallet from "./AppWallet";

export default {
  name: "App",
  components: {
    AppWallet
  },
  computed: {
    isWallet() {
      switch (window.location.hash.toLowerCase()) {
        case "#/debug":
          return false;
        default:
          return true;
      }
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
  font-size: 14px;
  -webkit-text-size-adjust: none;
  text-rendering: optimizeLegibility;
  font-variant-ligatures: none;
  -webkit-font-smoothing: antialiased;
}
</style>

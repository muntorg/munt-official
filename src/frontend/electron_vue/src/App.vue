<template>
  <div id="app">
    <component :is="app"></component>
  </div>
</template>

<script>
import AppMain from "./AppMain";
import AppDebug from "./AppDebug";

export default {
  data() {
    return {
      app: AppMain
    };
  },
  components: {
    AppMain,
    AppDebug
  },
  created() {
    switch (window.location.hash.toLowerCase()) {
      case "#/debug":
        this.app = AppDebug;
        break;
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
  font-size: 16px;
  line-height: 1.7em;
  -webkit-text-size-adjust: none;
  text-rendering: optimizeLegibility;
  font-variant-ligatures: none;
  -webkit-font-smoothing: antialiased;
}
</style>

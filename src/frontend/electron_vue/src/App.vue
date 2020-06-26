<template>
  <div id="app">
    <app-loader
      v-show="showLoader"
      :unityVersion="unityVersion"
      :walletVersion="walletVersion"
      :electronVersion="electronVersion"
      :isShuttingDown="isShuttingDown"
      :isSynchronizing="isSynchronizing"
    />

    <div v-show="!showLoader">
      <div class="app-top">
        <span class="app-logo"></span>
        <span class="app-balance" v-show="computedBalance !== null">{{
          computedBalance
        }}</span>

        <span class="top-menu" v-if="showSettings">
          <router-link :to="{ name: 'settings' }">
            <fa-icon :icon="['fal', 'cog']" />
            <span> {{ $t("settings.header") }}</span>
          </router-link>
        </span>
      </div>
      <div class="app-main">
        <div class="holder">
          <router-view />
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";
import { AppStatus } from "./store";
import AppLoader from "./components/AppLoader";

export default {
  data() {
    return {
      splashReady: false,
      electronVersion: process.versions.electron,
      progress: 1
    };
  },
  components: {
    AppLoader
  },
  watch: {
    status() {
      this.handleStatusChanged();
    }
  },
  computed: {
    ...mapState(["status", "unityVersion", "walletVersion", "balance"]),
    showSettings() {
      return this.status === AppStatus.ready;
    },
    computedBalance() {
      if (this.balance === undefined || this.balance === null) return null;
      return (
        (this.balance.availableIncludingLocked +
          this.balance.unconfirmedIncludingLocked +
          this.balance.immatureIncludingLocked) /
        100000000
      ).toFixed(2);
    },
    showLoader() {
      return (
        this.splashReady === false ||
        (this.status !== AppStatus.ready && this.status !== AppStatus.setup)
      );
    },
    isShuttingDown() {
      return this.status === AppStatus.shutdown;
    },
    isSynchronizing() {
      return this.status === AppStatus.synchronize;
    }
  },
  created() {
    this.handleStatusChanged();
  },
  mounted() {
    setTimeout(() => {
      this.splashReady = true;
    }, 2500);
  },
  methods: {
    handleStatusChanged() {
      let routeName;
      switch (this.status) {
        case AppStatus.setup:
          routeName = "setup";
          break;
        case AppStatus.synchronize:
        case AppStatus.ready:
          routeName = "wallet";
          break;
      }
      if (routeName === undefined || this.$route.name === routeName) return;
      this.$router.push({ name: routeName });
    }
  }
};
</script>

<style lang="less">
@import "./css/app.less";

:root {
  --top-height: 62px;
}

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

#app {
  width: 100%;
  height: 100%;
  color: #000;
  background-color: #fff;
}

.app-top {
  position: absolute;
  top: 0;
  height: var(--top-height);
  line-height: var(--top-height);
  width: 100%;
  padding: 0 40px 0 40px;
  background-color: #000;
  color: #fff;
}

.top-menu {
  float: right;
  margin-right: -15px;
}

.top-menu a,
.top-menu a:active,
.top-menu a:visited {
  display: inline-block;
  padding: 0 10px 0 10px;
  font-size: 0.9em;
  font-weight: 400;
  line-height: 32px;
  color: #fff;
}

.top-menu a:hover {
  background-color: #222;
}

.app-main {
  position: absolute;
  top: var(--top-height);
  height: calc(100% - var(--top-height));
  width: 100%;
  padding: 0 40px 100px 40px;
}

.holder {
  margin: 0 auto;
  width: 100%;
  max-width: 800px;
}

.app-logo {
  float: left;
  margin-top: 20px;
  width: 22px;
  height: 22px;
  background: url("./img/logo.svg"), linear-gradient(transparent, transparent);
}

.app-balance {
  float: left;
  margin-left: 10px;
}
</style>

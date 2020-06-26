<template>
  <div id="app">
    <!-- app loader -->
    <div id="app-loader" v-show="showLoader">
      <div class="logo-outer">
        <div class="logo-inner"></div>
      </div>
      <div class="version-container">
        <span>Unity: {{ unityVersion }}</span>
        <span class="divider">|</span>
        <span>Wallet: {{ walletVersion }}</span>
        <span class="divider">|</span>
        <span>Electron: {{ electronVersion }}</span>
      </div>
      <div class="info">
        <p v-show="isShuttingDown">
          {{ $t("loader.shutdown") }}
        </p>
        <div v-show="isSynchronizing">
          <!-- todo: add progress bar -->
          <div>{{ $t("loader.synchronizing") }}</div>
        </div>
      </div>
    </div>

    <!-- main application -->
    <div>
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
import UnityBackend from "./unity/UnityBackend";

let splashTimeout = 2500;
let synchronizeTimeout = null;

export default {
  data() {
    return {
      splashReady: false,
      electronVersion: process.versions.electron,
      progress: 1
    };
  },
  watch: {
    status() {
      if (this.status === AppStatus.synchronize) {
        this.synchronize();
      }
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
          this.balance.unconfirmedIncludingLocked + this.balance.immatureIncludingLocked) /
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
  mounted() {
    setTimeout(() => {
      this.splashReady = true;
    }, splashTimeout);
    if (this.status === AppStatus.synchronize) {
      this.synchronize();
    }
  },
  methods: {
    synchronize() {
      clearTimeout(synchronizeTimeout);

      let progress = UnityBackend.GetUnifiedProgress();
      progress = parseInt(parseFloat(progress) * 100);

      if (this.progress < progress) {
        this.progress = progress;
      }
      if (this.progress < 100) {
        synchronizeTimeout = setTimeout(this.synchronize, 1000);
      } else {
        synchronizeTimeout = setTimeout(() => {
          if (this.status !== AppStatus.shutdown) {
            this.$store.dispatch({
              type: "SET_STATUS",
              status: AppStatus.ready
            });
          }
        }, splashTimeout);
      }
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

#app-loader {
  position: absolute;
  width: 100%;
  height: 100%;
  margin-top: 0;
  margin-left: 0;
  display: flex;
  align-items: center;
  flex-direction: column;
  justify-content: center;
  background-color: #fff;
  z-index: 1;
}

#app-loader .version-container {
  margin: 16px 0;
}

#app-loader .version-container .divider {
  margin: 0 8px;
}

#app-loader .info {
  min-height: 100px;
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

.logo-outer {
  margin: 24px;
  background: #009572;
  width: 128px;
  height: 128px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  flex-direction: column;
  justify-content: center;
  text-align: center;
}

.logo-inner {
  width: 64px;
  height: 64px;
  background: url("./img/logo.svg");
  background-size: cover;
}

.app-balance {
  float: left;
  margin-left: 10px;
}
</style>

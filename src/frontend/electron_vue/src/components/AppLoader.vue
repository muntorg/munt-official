<template>
  <div class="app-loader flex-col" v-if="showLoader">
    <div class="logo-outer flex-col">
      <div class="logo-inner"></div>
    </div>
    <div class="version-container">
      <span>Florin </span>
      <span class="divider">|</span>
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
        <div class="sync-desc">{{ $t("loader.synchronizing") }}</div>
        <progress ref="progress" max="100" value="0"></progress>
      </div>
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";
import AppStatus from "../AppStatus";
import { LibraryController } from "../unity/Controllers";
import UIConfig from "../../ui-config.json";

let progressTimeout = null;
let progressCount = 0;

export default {
  name: "AppLoader",
  data() {
    return {
      electronVersion: process.versions.electron,
      progress: 0,
      UIConfig: UIConfig
    };
  },
  computed: {
    ...mapState("app", [
      "splashReady",
      "status",
      "unityVersion",
      "walletVersion"
    ]),
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
  watch: {
    status() {
      this.onStatusChanged();
    },
    progress() {
      this.$refs.progress.value = parseInt(this.progress * 100);
    }
  },
  created() {
    this.onStatusChanged();
  },
  mounted() {
    setTimeout(() => {
      this.$store.dispatch("app/SET_SPLASH_READY");
    }, 2500);
  },
  methods: {
    onStatusChanged() {
      if (this.UIConfig.showSidebar) {
        let routeName;
        switch (this.status) {
          case AppStatus.setup:
            routeName = "setup";
            break;
          case AppStatus.synchronize:
            routeName = "account";
            this.updateProgress();
            break;
          case AppStatus.ready:
            routeName = "account";
            break;
        }
        if (routeName === undefined || this.$route.name === routeName) return;
        this.$router.push({ name: routeName });
      } else {
        if (this.status === AppStatus.synchronize) {
          this.updateProgress();
        }
      }
    },
    updateProgress() {
      clearTimeout(progressTimeout);
      if (this.status !== AppStatus.synchronize) return;
      this.progress = LibraryController.GetUnifiedProgress();
      if (
        this.progress === 1 ||
        (this.progress === 0 && progressCount++ === 5) // when progress doesn't update, set status ready after 5 updates to prevent a endless loading screen
      ) {
        this.$store.dispatch("app/SET_STATUS", AppStatus.ready);
      } else {
        progressTimeout = setTimeout(this.updateProgress, 2500);
      }
    }
  }
};
</script>

<style lang="less" scoped>
.app-loader {
  position: absolute;
  width: 100%;
  height: 100%;
  margin-top: 0;
  margin-left: 0;
  align-items: center;
  justify-content: center;
  background-color: #fff;
  z-index: 9999;
}

.logo-outer {
  margin: 0 0 50px 0;
  background: var(--secondary-color);
  width: 128px;
  height: 128px;
  border-radius: 50%;
  align-items: center;
  justify-content: center;
  text-align: center;
}

.logo-inner {
  width: 98px;
  height: 98px;
  background: url("../img/logo.svg");
  background-size: cover;
}

.version-container {
  font-size: 0.8em;
  text-transform: uppercase;
  color: #999;
  margin-bottom: 20px;

  & .divider {
    margin: 0 8px;
  }
}

.info {
  margin: 0 0 50px 0;
}

.sync-desc {
  margin: 0 0 10px 0;
}

progress[value] {
  appearance: none;
  width: 100%;
  height: 5px;
}

progress[value]::-webkit-progress-bar {
  background-color: #efefef;
}

progress[value]::-webkit-progress-value {
  background-color: var(--primary-color);
}
</style>

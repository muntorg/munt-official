<template>
  <div class="app-loader flex-col" v-if="showLoader">
    <div class="logo-outer flex-col">
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
</template>

<script>
import { mapState } from "vuex";
import AppStatus from "../AppStatus";

export default {
  name: "AppLoader",
  data() {
    return {
      splashReady: false,
      electronVersion: process.versions.electron
    };
  },
  computed: {
    ...mapState("app", ["status", "unityVersion", "walletVersion"]),
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
    }
  },
  created() {
    this.onStatusChanged();
  },
  mounted() {
    setTimeout(() => {
      this.splashReady = true;
    }, 2500);
  },
  methods: {
    onStatusChanged() {
      let routeName;
      switch (this.status) {
        case AppStatus.setup:
          routeName = "setup";
          break;
        case AppStatus.synchronize:
        case AppStatus.ready:
          routeName = "account";
          break;
      }
      if (routeName === undefined || this.$route.name === routeName) return;
      this.$router.push({ name: routeName });
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
  margin: 24px;
  background: var(--primary-color);
  width: 128px;
  height: 128px;
  border-radius: 50%;
  align-items: center;
  justify-content: center;
  text-align: center;
}

.logo-inner {
  width: 64px;
  height: 64px;
  background: url("../img/logo.svg");
  background-size: cover;
}

.version-container {
  margin: 16px 0;

  & .divider {
    margin: 0 8px;
  }
}

.info {
  min-height: 100px;
}
</style>

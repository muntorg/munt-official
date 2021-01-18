<template>
  <div class="app-loader flex-col">
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
        <div>{{ $t("loader.synchronizing") }}</div>
        <progress ref="progress" max="100" value="0"></progress>
      </div>
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";
import AppStatus from "../AppStatus";
import { LibraryController } from "../unity/Controllers";

let progressTimeout = null;
let progressCount = 0;

export default {
  name: "AppLoader",
  data() {
    return {
      electronVersion: process.versions.electron,
      progress: 0
    };
  },
  computed: {
    ...mapState("app", ["status", "unityVersion", "walletVersion"]),
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
      if (this.status === AppStatus.synchronize) {
          this.updateProgress();
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

progress[value] {
  appearance: none;
  width: 100%;
  height: 4px;
}

progress[value]::-webkit-progress-bar {
  background-color: #eee;
}

progress[value]::-webkit-progress-value {
  background-color: var(--primary-color);
}
</style>

<template>
  <div class="app-loader flex-col" v-if="showLoader">
    <div class="logo-outer flex-col">
      <div class="logo-inner"></div>
    </div>
    <div class="info">
      <p v-show="isShuttingDown">
        {{ $t("loader.shutdown") }}
      </p>
      <div v-show="isSynchronizing">
        <div class="sync-desc">{{ $t("loader.synchronizing") }}</div>
        <progress ref="progress" max="130" value="0"></progress>
      </div>
    </div>
    <div class="version-container">
      <span>Gulden LITE</span>
      <span class="divider">|</span>
      <span>Unity: {{ unityVersion }}</span>
      <span class="divider">|</span>
      <span>Wallet: {{ walletVersion }}</span>
      <span class="divider">|</span>
      <span>Electron: {{ electronVersion }}</span>
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";
import AppStatus from "../AppStatus";
import { LibraryController } from "../unity/Controllers";

let progressTimeout = null;

// How many times we have polled for progress while the app is still in a loading state and is not yet synchronising
let preProgressCount = 0;

export default {
  name: "AppLoader",
  data() {
    return {
      electronVersion: process.versions.electron,
      progress: 0
    };
  },
  computed: {
    ...mapState("app", ["splashReady", "syncDone", "status", "unityVersion", "walletVersion"]),
    showLoader() {
      return this.splashReady === false || (this.status !== AppStatus.setup && this.syncDone === false) || this.status === AppStatus.shutdown;
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
      if (this.$refs.progress) {
        this.$refs.progress.value = parseInt(this.progress * 100);
      }
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
    },
    updateProgress() {
      clearTimeout(progressTimeout);
      if (this.status !== AppStatus.synchronize) return;

      const progress = LibraryController.GetUnifiedProgress();
      console.log(`progress: ${progress}`);

      // App goes through two basic loading processes:
      // a. Setting up wallet, loading transactions/accounts, starting up of internal services network threads etc.
      // b. Synchronising with network
      // While we are in 'a' its not possible to determine progress and we get only a response of 200 from 'GetUnifiedProgress' to indicate this
      // Once we are in 'b' we get a fraction of between 0..1 expressing the percentage we are in terms of complete.
      //
      // To deal with this we do the following:
      // 1. We make our total progress target "130" instead of "100"
      // 2. The first 30 of that 130 we proportion to "a" the second 100 we proportion to "b"
      // 3. While in a we slowly increment over time from 0..30 to show the user progress is taking place
      // 4. While in b we assume we are already starting from 30 and just add the return value of 'GetUnifiedProgress' on top of that
      //
      // Room for improvement:
      // 1. Instead of starting b from 30 we should determine where 'a' left off (e.g. maybe it was only at 10) and start from there, and drop the progress total to match.
      // 2. Improve the API call to return an object that properly specifies this instead of the magic "200" number
      // 3. Fine tune what percentage of the sync we proportion to "a"
      // 4. Use different proportions if we are performing an initial sync on a new wallet vs catching up on a previously synced wallet
      // 'b' takes substantially longer on 'b' while if I open/close a recently synced wallet most time will be spent in 'a'

      if (!this.syncDone) {
        if (progress > 190) {
          this.progress = (0.3 / 20) * preProgressCount;
          if (preProgressCount < 20) {
            preProgressCount++;
          }
          progressTimeout = setTimeout(this.updateProgress, 1000);
        } else {
          this.progress = 0.3 + progress;
          progressTimeout = setTimeout(this.updateProgress, 2500);
        }
      } else {
        // we always wait for syncDone to be true before continuing
        this.progress = 1.3;
        this.$store.dispatch("app/SET_STATUS", AppStatus.ready);
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
  background: url("../img/logo-start.svg");
  background-size: cover;
}

.version-container {
  font-size: 0.8em;
  text-transform: uppercase;
  color: #999;

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

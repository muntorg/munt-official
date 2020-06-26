<template>
  <div class="app-loader">
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
</template>

<script>
export default {
  name: "AppLoader",
  props: {
    unityVersion: {
      type: String,
      default: "0.0.0"
    },
    walletVersion: {
      type: String,
      default: "0.0.0"
    },
    electronVersion: {
      type: String,
      default: "0.0.0"
    },
    isShuttingDown: {
      type: Boolean,
      default: false
    },
    isSynchronizing: {
      type: Boolean,
      default: false
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
  display: flex;
  align-items: center;
  flex-direction: column;
  justify-content: center;
  background-color: #fff;
  z-index: 9999;
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

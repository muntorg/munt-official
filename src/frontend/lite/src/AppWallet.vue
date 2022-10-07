<template>
  <div class="app-wallet">
    <app-loader />
    <modal-dialog v-model="modal" />
    <unlock-wallet-dialog />
    <activity-indicator v-if="activityIndicator" />
    <component :is="layout">
      <router-view />
    </component>
  </div>
</template>

<script>
import { mapState } from "vuex";
import AppStatus from "./AppStatus";
import AppLoader from "./components/AppLoader";
import ModalDialog from "./components/ModalDialog";
import EventBus from "./EventBus.js";
import ActivityIndicator from "./components/ActivityIndicator.vue";
import UnlockWalletDialog from "./components/UnlockWalletDialog";

import SetupLayout from "./layouts/SetupLayout";
import WalletLayout from "./layouts/WalletLayout";

export default {
  name: "AppWallet",
  data() {
    return {
      modal: null
    };
  },
  components: {
    AppLoader,
    ModalDialog,
    SetupLayout,
    WalletLayout,
    ActivityIndicator,
    UnlockWalletDialog
  },
  created() {
    this.onStatusChanged();
  },
  mounted() {
    EventBus.$on("close-dialog", this.closeModal);
    EventBus.$on("show-dialog", this.showModal);
  },
  beforeDestroy() {
    EventBus.$off("close-dialog", this.closeModal);
    EventBus.$off("show-dialog", this.showModal);
  },
  computed: {
    ...mapState("app", ["status", "activityIndicator"]),
    layout() {
      return this.$route.meta.layout || WalletLayout;
    }
  },
  watch: {
    status() {
      this.onStatusChanged();
    }
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
          break;
        case AppStatus.ready:
          routeName = "account";
          break;
      }
      if (routeName === undefined || this.$route.name === routeName) return;
      this.$router.push({ name: routeName });
    },
    closeModal() {
      this.modal = null;
    },
    showModal(modal) {
      this.modal = modal;
    }
  }
};
</script>

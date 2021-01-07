<template>
  <div class="app-wallet">
    <app-loader />
    <modal-dialog v-model="modal" />
    <component :is="layout">
      <router-view />
    </component>
  </div>
</template>

<script>
import AppLoader from "./components/AppLoader";
import ModalDialog from "./components/ModalDialog";
import EventBus from "./EventBus.js";

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
    WalletLayout
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
    layout() {
      return this.$route.meta.layout || WalletLayout;
    }
  },
  methods: {
    closeModal() {
      this.modal = null;
    },
    showModal(modal) {
      this.modal = modal;
    }
  }
};
</script>

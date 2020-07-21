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

const default_layout = "wallet-layout";

export default {
  name: "AppWallet",
  data() {
    return {
      isMounted: false,
      modal: null
    };
  },
  components: {
    AppLoader,
    ModalDialog
  },
  mounted() {
    if (this.isMounted) return;
    this.isMounted = true;
    EventBus.$on("show-dialog", this.showModal);
  },
  computed: {
    layout() {
      return this.$route.meta.layout || default_layout;
    }
  },
  methods: {
    showModal(modal) {
      this.modal = modal;
    }
  }
};
</script>

<template>
  <div class="app-debug">
    <modal-dialog v-model="modal" />
    <div class="topbar flex-row">
      <div v-for="(tab, index) in tabs" :key="index" :class="getTabClass(index)" @click="setTab(index)">
        {{ tab.title }}
      </div>
    </div>
    <div class="main scrollable">
      <information-page v-if="current === 0" />
      <debug-console :show="current === 1" />
      <peers-page v-if="current === 2" />
    </div>
  </div>
</template>

<script>
import InformationPage from "./InformationPage";
import DebugConsole from "./DebugConsole";
import PeersPage from "./PeersPage";
import ModalDialog from "../../components/ModalDialog";
import EventBus from "../../EventBus.js";

export default {
  data() {
    return {
      current: 0,
      tabs: [
        {
          title: "Information"
        },
        {
          title: "Console"
        },
        {
          title: "Peers"
        }
      ],
      modal: null
    };
  },
  components: {
    InformationPage,
    DebugConsole,
    PeersPage,
    ModalDialog
  },
  mounted() {
    EventBus.$on("close-dialog", this.closeModal);
    EventBus.$on("show-dialog", this.showModal);
  },
  beforeDestroy() {
    EventBus.$off("close-dialog", this.closeModal);
    EventBus.$off("show-dialog", this.showModal);
  },
  methods: {
    getTabClass(index) {
      return index === this.current ? "active" : "";
    },
    setTab(index) {
      this.current = index;
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

<style lang="less" scoped>
.app-debug {
  width: 100%;
  height: 100vh;
  overflow: hidden;
}

.topbar {
  width: 100%;
  height: 56px;
  color: #000;
  border-bottom: 1px solid var(--main-border-color);

  & div {
    padding: 0 20px;
    font-weight: 600;
    font-size: 0.85em;
    line-height: 56px;
    text-transform: uppercase;
    letter-spacing: 0.03em;
    cursor: pointer;

    &:hover {
      color: var(--primary-color);
      background: var(--hover-color);
    }

    &.active,
    .active:hover {
      color: var(--primary-color);
    }
  }
}

.main {
  height: calc(100% - 48px);
  padding: 20px;
  overflow-x: hidden;
  overflow-y: auto;
}
</style>

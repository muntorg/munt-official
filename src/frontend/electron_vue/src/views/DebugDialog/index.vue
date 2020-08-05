<template>
  <div class="app-debug">
    <div class="topbar flex-row">
      <div
        v-for="(tab, index) in tabs"
        :key="index"
        :class="getTabClass(index)"
        @click="setTab(index)"
      >
        {{ tab.title }}
      </div>
    </div>
    <div class="main">
      <information-page v-if="current === 0" />
      <debug-console
        v-if="current === 1"
        :value="console"
        :output="console.output"
        :commands="console.commands"
        @clear-output="clearOutput"
      />
    </div>
  </div>
</template>

<script>
import InformationPage from "./InformationPage";
import DebugConsole from "./DebugConsole";

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
        }
      ],
      console: {
        output: [],
        commands: [],
        idx: 0
      }
    };
  },
  components: {
    InformationPage,
    DebugConsole
  },
  methods: {
    getTabClass(index) {
      return index === this.current ? "active" : "";
    },
    setTab(index) {
      this.current = index;
    },
    clearOutput() {
      this.console.output = [];
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
  height: 48px;
  background-color: #000;
  color: #fff;

  & div {
    cursor: pointer;
    padding: 0px 20px;
    height: 100%;
    line-height: 48px;

    &:hover {
      background: #333;
    }

    &.active,
    .active:hover {
      background: var(--primary-color);
    }
  }
}

.main {
  height: calc(100% - 48px);
  padding: 12px;
  overflow-x: hidden;
  overflow-y: auto;
}
</style>

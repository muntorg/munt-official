<template>
  <div class="app-debug">
    <div class="topbar">
      <span
        v-for="(tab, index) in tabs"
        :key="index"
        :class="getTabClass(index)"
        @click="setTab(index)"
      >
        {{ tab.title }}
      </span>
    </div>
    <div class="main">
      <debug-console v-show="current === 0" />
    </div>
  </div>
</template>

<script>
import DebugConsole from "./components/DebugConsole";

export default {
  name: "AppDebug",
  data() {
    return {
      current: 0,
      tabs: [
        {
          title: "Console"
        }
      ]
    };
  },
  components: {
    DebugConsole
  },
  methods: {
    getTabClass(index) {
      return index === this.current ? "active" : "";
    },
    setTab(index) {
      this.current = index;
    }
  }
};
</script>

<style lang="less" scoped>
.app-debug {
  width: 100%;
  height: 100vh;
}

.topbar {
  width: 100%;
  height: 48px;
  background-color: #000;
  color: #fff;

  display: flex;
  flex-direction: row;

  & span {
    cursor: pointer;
    padding: 10px 20px;
    height: 100%;

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
}
</style>

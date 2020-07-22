<template>
  <div class="app-debug">
    <div class="topbar">
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
      <debug-console v-show="current === 0" />
    </div>
  </div>
</template>

<script>
import DebugConsole from "./DebugConsole";

export default {
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
}
</style>

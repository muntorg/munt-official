<template>
  <div class="main-header" :class="mainHeaderClass">
    <div v-if="UIConfig.showHeaderLogo" class="logo" />
    <div>
      <div class="title ellipsis">
        {{ title }}
      </div>
      <div class="subtitle ellipsis" v-if="hasSubtitle">
        {{ subtitle }}
      </div>
    </div>
  </div>
</template>

<script>
import UIConfig from "../../../ui-config.json";

export default {
  name: "MainHeader",
  props: {
    title: null,
    subtitle: undefined
  },
  computed: {
    hasSubtitle() {
      return this.subtitle !== undefined;
    },
    mainHeaderClass() {
      return this.hasSubtitle ? "" : "no-subtitle";
    }
  },
  data() {
    return {
      UIConfig: UIConfig
    };
  }
};
</script>

<style lang="less" scoped>
.main-header {
  height: var(--header-height);
  line-height: 20px;
  padding: calc((var(--header-height) - 40px) / 2) 0;
  display: flex;
  flex-direction: row;
  align-items: center;

  &.no-subtitle {
    padding: calc((var(--header-height) - 20px) / 2) 0;
  }
}

& .logo {
  width: 22px;
  min-width: 22px;
  height: 22px;
  min-height: 22px;
  background: url("../../img/logo.svg"), linear-gradient(transparent, transparent);
  background-size: cover;
  margin-right: 10px;
}

.title {
  font-size: 1.1em;
  font-weight: 500;
  line-height: 20px;
}
</style>

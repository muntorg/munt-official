<template>
  <div class="main-header" :class="mainHeaderClass">
    <div class="title ellipsis">
      {{ title }}
    </div>
    <div class="subtitle ellipsis" v-if="hasSubtitle">
      {{ subtitle }}
    </div>
  </div>
</template>

<script>
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
  }
};
</script>

<style lang="less" scoped>
.main-header {
  height: var(--header-height);
  line-height: 20px;
  padding: calc((var(--header-height) - 40px) / 2) 0;

  &.no-subtitle {
    padding: calc((var(--header-height) - 20px) / 2) 0;
  }
}

.title {
  font-size: 1.1em;
  font-weight: 500;
  line-height: 20px;
}
</style>

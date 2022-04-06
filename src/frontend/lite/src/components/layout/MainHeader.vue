<template>
  <div class="main-header" :class="mainHeaderClass">
    <div class="title ellipsis">
      {{ $t(title) }}
    </div>
    <div class="subtitle ellipsis" v-if="hasSubtitle">
      {{ $t(subtitle) }}
    </div>
  </div>
</template>

<script>
export default {
  name: "MainHeader",
  props: {
    title: {
      type: String,
      default: null
    },
    subtitle: {
      type: String,
      default: null
    }
  },
  computed: {
    hasSubtitle() {
      return this.subtitle && this.subtitle.trim().length > 0;
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
  display: flex;
  flex-direction: row;
  align-items: center;

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

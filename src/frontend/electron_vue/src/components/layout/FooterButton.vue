<template>
  <div :class="getClass(isActive)" @click="$emit('click', routeName)">
    <fa-icon v-if="icon" :icon="icon" />
    <slot></slot>
  </div>
</template>

<script>
export default {
  name: "FooterButton",
  props: {
    routeName: {
      type: String
    },
    params: {
      type: Object,
      value: () => {}
    },
    icon: {
      type: Array,
      value: () => []
    }
  },
  computed: {
    isActive() {
      return this.$route.name === this.routeName;
    }
  },
  methods: {
    getClass(isActive) {
      return `footer-button ${isActive ? "active" : ""}`;
    }
  }
};
</script>

<style lang="less" scoped>
.footer-button {
  display: inline-block;
  padding: 0 30px;
  font-weight: 600;
  font-size: 0.85em;
  text-transform: uppercase;
  letter-spacing: 0.03em;
  line-height: calc(var(--footer-height) - 20px);
  cursor: pointer;

  & svg {
    font-size: 14px;
    margin-right: 5px;
  }

  &.active {
    color: var(--primary-color);
    cursor: default;
  }

  &:not(.active):hover {
    background: var(--hover-color);
    color: var(--primary-color);
  }
}
</style>

<template>
  <div :class="getClass(isActive)" @click="$emit('click', routeName)">
    <fa-icon class="left" v-if="showIcon('left')" :icon="icon" />
    <slot></slot>
    <fa-icon class="right" v-if="showIcon('right')" :icon="icon" />
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
    },
    iconPosition: {
      type: String,
      default: "left",
      validator: value => {
        return ["left", "right"].includes(value);
      }
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
    },
    showIcon(position) {
      if (!this.icon) return false;
      return this.iconPosition === position;
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
  }

  & svg.left {
    margin-right: 5px;
  }

  & svg.right {
    margin-left: 5px;
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

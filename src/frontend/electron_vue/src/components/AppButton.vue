<template>
  <button
    class="app-button"
    :class="type"
    :disabled="disabled"
    @click="onClick($event)"
  >
    <slot></slot>
  </button>
</template>

<script>
export default {
  name: "AppButton",
  props: {
    disabled: {
      type: Boolean,
      default: false
    },
    type: {
      type: String,
      default: ""
    }
  },
  methods: {
    onClick(e) {
      this.$emit("click", e);
    }
  }
};
</script>

<style lang="less" scoped>
.app-button {
  padding: 0 20px 0 20px;
  font-style: normal;
  font-weight: 600;
  font-size: 0.75em;
  line-height: 40px;
  text-transform: uppercase;
  letter-spacing: 0.03em;
  text-align: center;
  text-decoration: none;
  border: 0;
  cursor: pointer;
  -webkit-appearance: none;
  -webkit-font-smoothing: antialiased !important;
  transition: all 0.3s;
  border-radius: 0px;
  color: #fff;
  background-color: #009572;

  &:hover {
    background-color: #00a880;
  }

  &[disabled="disabled"],
  &[disabled="disabled"]:hover {
    background-color: #eee;
    cursor: default;
  }
}
</style>

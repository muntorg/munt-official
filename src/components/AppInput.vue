<template>
  <input
    class="app-input"
    :class="status"
    :type="type"
    :disabled="disabled"
    :value="value"
    @focus="$emit('focus', $event)"
    @keydown="$emit('keydown', $event)"
    @keyup="$emit('keyup', $event)"
    @input="$emit('input', $event.target.value)"
  />
</template>

<script>
export default {
  name: "AppInput",
  props: {
    value: String,
    disabled: {
      type: Boolean,
      default: false
    },
    type: {
      type: String,
      default: "text"
    },
    status: {
      type: String,
      default: ""
    }
  }
};
</script>

<style lang="less" scoped>
.app-input {
  font-style: normal;
  font-weight: 400;
  line-height: 22px;
  font-size: 16px;
  height: 40px;
  width: 100%;
  padding: 10px;
  transition: all 0.3s;
  border-radius: 0px;
  border: 1px solid #ccc;
  color: #000;
  background-color: #fff;

  &:focus,
  &.success,
  &:focus.success {
    color: #009572;
    border-color: #009572;
  }

  &.error,
  &:focus.error {
    color: #dd3333;
    border-color: #dd3333;
  }
}
</style>

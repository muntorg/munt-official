<template>
  <div class="novo-button-section">
    <div class="left">
      <slot name="left" />
    </div>
    <div class="middle"></div>
    <div class="right">
      <slot />
      <slot name="right" />
    </div>
  </div>
</template>

<script>
export default {
  name: "NovoButtonSection"
};
</script>

<style lang="less" scoped>
.novo-button-section {
  display: flex;
  width: 100%;
  flex-direction: row;

  & .left {
    float: left;
    & > button:not(:last-child) {
      margin: 0 20px 0 0;
    }
  }

  & .middle {
    flex: 1;
  }

  & .right {
    text-align: right;
    & > button:not(:first-child) {
      margin: 0 0 0 20px;
    }
  }
}
</style>

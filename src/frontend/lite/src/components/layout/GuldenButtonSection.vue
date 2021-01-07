<template>
  <div class="gulden-button-section flex-row">
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
  name: "GuldenButtonSection"
};
</script>

<style lang="less" scoped>
.gulden-button-section {
  width: 100%;

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

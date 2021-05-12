<template>
  <div class="section">
    <slot />
  </div>
</template>

<style lang="less" scoped>
.section {
  margin: 0 0 20px 0;
}
</style>

<script>
export default {
  name: "AppSection"
};
</script>

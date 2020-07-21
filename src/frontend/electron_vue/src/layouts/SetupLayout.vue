<template>
  <div class="setup-layout">
    <div class="header">
      <div class="logo" />
    </div>
    <div class="main">
      <router-view />
    </div>
  </div>
</template>

<script>
export default {
  name: "SetupLayout"
};
</script>

<style lang="less" scoped>
.setup-layout {
  width: 100%;
  height: 100vh;
  overflow: hidden;

  & .header {
    height: var(--header-height);
    border-bottom: 1px solid #ddd;
    padding: 20px;

    & .logo {
      width: 22px;
      height: 22px;
      background: url("../img/logo-start.svg"),
        linear-gradient(transparent, transparent);
      background-size: cover;
    }
  }

  & .main {
    height: calc(100vh - var(--header-height));
    padding: 40px;
  }
}
</style>

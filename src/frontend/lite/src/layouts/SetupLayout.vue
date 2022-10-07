<template>
  <section class="setup-layout">
    <section class="header">
      <div class="logo" />
    </section>
    <section class="main">
      <router-view />
    </section>
  </section>
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
    border-bottom: 1px solid var(--main-border-color);
    padding: 20px;
    font-size: 1.1em;
    line-height: 42px;

    & .logo {
      width: 22px;
      height: 22px;
      background: url("../img/logo-start.svg"), linear-gradient(transparent, transparent);
      background-size: cover;
    }
  }

  & .main {
    height: calc(100vh - var(--header-height));
    padding: 40px;
  }
}
</style>

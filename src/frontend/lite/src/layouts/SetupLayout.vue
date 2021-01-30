<template>
  <section class="wallet-layout">
    <section class="header flex-row">
      <div class="logo" />
    </section>
    <section class="content scrollable">
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
.wallet-layout {
  height: 100vh;
  overflow: hidden;

  & > .header {
    height: var(--header-height);
    padding: 10px 20px 10px 20px;
    border-bottom: 1px solid var(--main-border-color);
    font-size: 1.1em;
    line-height: 42px;

    & > .logo {
      margin: 10px;
      width: 22px;
      min-width: 22px;
      height: 22px;
      min-height: 22px;
      background: url("../img/logo-start.svg"),
        linear-gradient(transparent, transparent);
      background-size: cover;
    }
  }

  & > .content {
    height: calc(100% - var(--header-height));
    padding: 40px 30px 30px 30px;
  }
}
</style>

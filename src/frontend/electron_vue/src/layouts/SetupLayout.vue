<template>
  <div class="setup-layout">
    <router-view />

    <!--
    <div class="app-topbar">
      <span class="app-topbar--logo"></span>
      <span class="app-topbar--balance" v-show="totalBalance !== null">{{
        totalBalance
      }}</span>
      <span class="app-topbar--settings" v-if="showSettings">
        <router-link :to="{ name: 'settings' }">
          <fa-icon :icon="['fal', 'cog']" />
          <span> {{ $t("settings.header") }}</span>
        </router-link>
      </span>
    </div>
    <div class="app-content">
      <div class="app-content--wrapper">
        <router-view />
      </div>
    </div>
    -->
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
}

/*
.app-topbar {
  position: fixed;
  top: 0;
  height: var(--top-height);
  line-height: var(--top-height);
  width: 100%;
  padding: 0 40px 0 40px;
  background-color: #000;
  color: #fff;
  z-index: 999;

  & .app-topbar--logo {
    float: left;
    margin-top: 20px;
    width: 22px;
    height: 22px;
    background: url("./img/logo.svg"), linear-gradient(transparent, transparent);
  }

  & .app-topbar--balance {
    float: left;
    margin-left: 10px;
  }

  & .app-topbar--settings {
    float: right;
    margin-right: -10px;
  }

  & .app-topbar--settings a,
  .app-topbar--settings a:active,
  .app-topbar--settings a:visited {
    display: inline-block;
    padding: 0 10px 0 10px;
    font-size: 0.9em;
    font-weight: 400;
    line-height: 32px;
    color: #fff;
  }

  .app-topbar--settings a:hover {
    background-color: #222;
  }
}

.app-content {
  position: absolute;
  top: var(--top-height);
  height: calc(100% - var(--top-height));
  width: 100%;
  padding: 0 40px 100px 40px;

  & .app-content--wrapper {
    margin: 40px auto;
    width: 100%;
    max-width: 800px;
  }
}
*/
</style>

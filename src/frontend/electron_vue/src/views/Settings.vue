<template>
  <div class="settings-view">
    <novo-section class="back">
      <router-link :to="{ name: 'wallet' }">
        <fa-icon :icon="['fal', 'long-arrow-left']" />
        <span> {{ $t("buttons.back") }}</span>
      </router-link>
    </novo-section>
    <router-view />
  </div>
</template>

<style scoped lang="less">
.back a {
  padding: 4px 8px;
  margin: 0 0 0 -8px;

  &:hover {
    background-color: #f5f5f5;
  }
}
</style>

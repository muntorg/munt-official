<template>
  <div class="settings-view">
    <div class="back">
      <router-link :to="{ name: 'wallet' }">
        <fa-icon :icon="['fal', 'long-arrow-left']" />
        <span> {{ $t("buttons.back") }}</span>
      </router-link>
    </div>
    <router-view />
  </div>
</template>

<style scoped lang="less">
.back a {
  padding: 4px 8px;
  margin: 0 0 0 -8px;
}
.back {
  margin-bottom: 20px;
}

.back a:hover {
  background-color: #f5f5f5;
}
</style>

<script>
export default {};
</script>

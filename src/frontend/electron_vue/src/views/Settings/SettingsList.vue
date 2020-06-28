<template>
  <div class="settings-list-view">
    <h2>{{ $t("settings.header") }}</h2>
    <div class="settings-row">
      <router-link :to="{ name: 'view-recovery-phrase' }">
        {{ $t("settings.view_recovery_phrase") }}
        <fa-icon :icon="['fal', 'long-arrow-right']" class="arrow" />
      </router-link>
    </div>
    <div class="settings-row">
      <router-link :to="{ name: 'change-password' }">
        {{ $t("settings.change_password") }}
        <fa-icon :icon="['fal', 'long-arrow-right']" class="arrow" />
      </router-link>
    </div>
  </div>
</template>

<style lang="less" scoped>
.settings-row {
  padding: 4px 0;
  border-bottom: 1px solid #ccc;
}

.settings-row a {
  display: inline-block;
  width: 100%;
}

.arrow {
  float: right;
  margin: 6px 0 0 0;
  color: #fff;
}

a:hover > .arrow {
  color: #000;
}
</style>

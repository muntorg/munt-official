<template>
  <div class="settings-list-view">
    <h2>{{ $t("settings.header") }}</h2>
    <div class="settings-row">
      <router-link :to="{ name: 'view-recovery-phrase' }">
        {{ $t("settings.view_recovery_phrase") }}
        <fa-icon :icon="['fal', 'chevron-right']" class="arrow" />
      </router-link>
    </div>
    <div class="settings-row">
      <router-link :to="{ name: 'change-password' }">
        {{ $t("settings.change_password") }}
        <fa-icon :icon="['fal', 'chevron-right']" class="arrow" />
      </router-link>
    </div>
  </div>
</template>

<style lang="less" scoped>
.settings-row {
  margin: 0 -10px;
  padding: 10px;
  cursor: pointer;
}

.settings-row:hover {
  color: var(--primary-color);
  background-color: #eff3ff;
}

.settings-row a {
  display: inline-block;
  width: 100%;
}

.arrow {
  float: right;
}
</style>

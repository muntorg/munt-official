<template>
  <div class="settings-list-view">
    <portal to="header-slot">
      <main-header :title="$t('settings.header')" />
    </portal>
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

    <portal to="footer-slot">
      <div />
    </portal>
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
  line-height: 20px;
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

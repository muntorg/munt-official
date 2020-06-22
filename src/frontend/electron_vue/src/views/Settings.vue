<template>
  <div class="settings">
    <div class="section">
      <div class="back">
        <router-link :to="{ name: 'wallet' }">
          <fa-icon :icon="['fal', 'long-arrow-left']" />
          <span> {{ $t("buttons.back") }}</span>
        </router-link>
      </div>

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

<script>
export default {};
</script>

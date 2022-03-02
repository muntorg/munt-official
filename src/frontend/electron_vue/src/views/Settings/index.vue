<template>
  <div class="settings-main" v-if="UIConfig.showSidebar">
    <portal to="header-slot">
      <main-header :title="$t('settings.header')" />
    </portal>
    <router-view />
  </div>
  <div v-else>
    <router-view />
  </div>
</template>

<script>
import UIConfig from "../../../ui-config.json";

export default {
  data() {
    return {
      UIConfig: UIConfig
    };
  }
};
</script>
<style>
.settings-main {
  height: 100%;
}
</style>

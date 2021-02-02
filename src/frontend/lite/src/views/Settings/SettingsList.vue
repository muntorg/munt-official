<template>
  <div class="settings-list-view">
    <h2>{{ $t("settings.header") }}</h2>
    <router-link :to="{ name: 'view-recovery-phrase' }">
      <div class="settings-row">
        {{ $t("settings.view_recovery_phrase") }}
        <fa-icon :icon="['fal', 'chevron-right']" class="arrow" />
      </div>
    </router-link>
    <router-link :to="{ name: 'change-password' }">
      <div class="settings-row">
        {{ $t("settings.change_password") }}
        <fa-icon :icon="['fal', 'chevron-right']" class="arrow" />
      </div>
    </router-link>
    <div class="settings-row flex-row">
      <div class="flex-1">{{ $t("settings.choose_theme") }}</div>
      <div
        :class="getThemeSelectClassNames('blue')"
        @click="switchTheme('blue')"
      ></div>
      <div
        :class="getThemeSelectClassNames('orange')"
        @click="switchTheme('orange')"
      ></div>
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";

export default {
  computed: {
    ...mapState("app", ["theme"])
  },
  methods: {
    getThemeSelectClassNames(theme) {
      const classNames = ["theme-select", theme];
      if (theme === "blue" && this.theme !== "orange")
        classNames.push("selected");
      else if (theme === "orange" && this.theme === "orange")
        classNames.push("selected");
      return classNames.join(" ");
    },
    switchTheme(theme) {
      this.$store.dispatch("app/SET_THEME", theme);
    }
  }
};
</script>

<style lang="less" scoped>
.settings-row {
  margin: 0 -10px;
  padding: 10px;
}

a > .settings-row:hover {
  color: var(--primary-color);
  background-color: var(--hover-color);
  cursor: pointer;
}

.arrow {
  float: right;
}

.theme-select {
  border: 2px solid #fff;
  border-radius: 18px;
  height: 20px;
  width: 20px;
  cursor: pointer;
  margin-left: 5px;
}

.theme-select.blue {
  background-color: #0039cc;
}
.theme-select.orange {
  background-color: #ee6622;
}

.selected {
  box-shadow: 0 0 0 1px #000;
}
</style>

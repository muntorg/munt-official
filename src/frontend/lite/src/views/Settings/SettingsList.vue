<template>
  <div class="settings-list-view">
    <portal v-if="!isSingleAccount" to="header-slot">
      <main-header title="settings.header" />
    </portal>
    <h2 v-else>{{ $t("settings.header") }}</h2>

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
    <div>
      <div v-if="hasThemes" class="settings-row-no-hover flex-row">
        <div class="flex-1">{{ $t("settings.choose_theme") }}</div>
        <div v-for="(theme, index) in themes" :key="theme.name">
          <div :class="getThemeSelectClassNames(theme.name, index)" :style="getThemeStyle(theme.color)" @click="switchTheme(theme.name)"></div>
        </div>
      </div>
    </div>
    <div class="settings-row-no-hover flex-row">
      <div class="flex-1">{{ $t("settings.choose_language") }}</div>
      <div :class="`language-select ${this.language === 'en' ? 'selected' : ''}`" @click="changeLanguage('en')">
        {{ $t("settings.english") }}
      </div>
      <div :class="`language-select ${this.language === 'nl' ? 'selected' : ''}`" @click="changeLanguage('nl')">
        {{ $t("settings.dutch") }}
      </div>
    </div>
    <div class="settings-row-no-hover flex-row">
      <div class="flex-1">{{ $t("settings.choose_decimal_places") }}</div>
      <div :class="`decimal-select ${this.decimals === 2 ? 'selected' : ''}`" @click="changeDecimals(2)">
        2
      </div>
      <div :class="`decimal-select ${this.decimals === 3 ? 'selected' : ''}`" @click="changeDecimals(3)">
        3
      </div>
      <div :class="`decimal-select ${this.decimals === 4 ? 'selected' : ''}`" @click="changeDecimals(4)">
        4
      </div>
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";
import UIConfig from "../../../ui-config.json";

export default {
  data() {
    return {
      isSingleAccount: UIConfig.isSingleAccount,
      themes: UIConfig.themes
    };
  },
  computed: {
    ...mapState("app", ["theme", "language", "decimals"]),
    hasThemes() {
      return this.themes && this.themes.length;
    }
  },
  methods: {
    getThemeSelectClassNames(theme, index) {
      const classNames = ["theme-select"];
      if (theme === this.theme || (!this.theme && index === 0)) classNames.push("selected");
      return classNames.join(" ");
    },
    getThemeStyle(color) {
      return `background-color: ${color}`;
    },
    switchTheme(theme) {
      this.$store.dispatch("app/SET_THEME", theme);
    },
    changeLanguage(language) {
      this.$store.dispatch("app/SET_LANGUAGE", language);
      this.$forceUpdate();
    },
    changeDecimals(decimal) {
      this.$store.dispatch("app/SET_DECIMALS", decimal);
      this.$forceUpdate();
    },
    routeTo(route) {
      this.$router.push({ name: route });
    }
  }
};
</script>

<style lang="less" scoped>
.settings-list-view {
  display: flex;
  flex-direction: column;
}

.settings-row {
  margin: 0 -10px;
  padding: 10px;
}

.settings-row-no-hover {
  margin: 0 -10px;
  padding: 10px;
}

.settings-row:hover {
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

.language-select {
  font-size: 12px;
  border-radius: 18px;
  line-height: 20px;
  width: 40px;
  text-align: center;
  cursor: pointer;
  margin-left: 15px;
}

.language-select:hover {
  cursor: pointer;
}

.decimal-select {
  font-size: 12px;
  border-radius: 18px;
  line-height: 20px;
  width: 40px;
  text-align: center;
  cursor: pointer;
  margin-left: 15px;
}

.selected {
  box-shadow: 0 0 0 1px #000;
}
</style>

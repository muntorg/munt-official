<template>
  <div class="settings-list-view">
    <portal v-if="!UIConfig.showSidebar" to="header-slot">
      <main-header :title="$t('settings.header')" />
    </portal>
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
      <div v-if="UIConfig.hasThemes" class="settings-row flex-row">
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
    <div class="settings-row flex-row">
      <div class="flex-1">{{ $t("settings.choose_language") }}</div>
      <div
        :class="`language-select ${this.language === 'en' ? 'selected' : ''}`"
        @click="changeLanguage('en')"
      >
        {{ $t("settings.english") }}
      </div>
      <div
        :class="`language-select ${this.language === 'nl' ? 'selected' : ''}`"
        @click="changeLanguage('nl')"
      >
        {{ $t("settings.dutch") }}
      </div>
    </div>
    <div style="flex: 1" />
    <portal v-if="UIConfig.showSidebar" to="footer-slot">
      <div />
    </portal>
    <div v-else style="margin-top: 20px">
      <app-button-section class="buttons">
        <template v-slot:left>
          <button @click="routeTo('transactions')">
            {{ $t("buttons.back") }}
          </button>
        </template>
      </app-button-section>
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";
import UIConfig from "../../../ui-config.json";

export default {
  computed: {
    ...mapState("app", ["theme", "language"])
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
    },
    changeLanguage(language) {
      this.$store.dispatch("app/SET_LANGUAGE", language);
      this.$forceUpdate();
    },
    routeTo(route) {
      this.$router.push({ name: route });
    }
  },
  data() {
    return {
      UIConfig: UIConfig
    };
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

.language-select {
  border: 2px solid #fff;
  border-radius: 18px;
  height: 20px;
  width: 40px;
  text-align: center;
  cursor: pointer;
  margin-left: 15px;
}

.language-select.en {
  // background-color: #0039cc;
}

.language-select.nl {
  // background-color: #ee6622;
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

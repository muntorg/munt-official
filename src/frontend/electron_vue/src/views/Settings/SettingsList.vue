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
      <div :class="`decimal-select ${this.decimals === 2 ? 'selected' : ''}`" @click="changeDecimals(2)">2</div>
      <div :class="`decimal-select ${this.decimals === 3 ? 'selected' : ''}`" @click="changeDecimals(3)">3</div>
      <div :class="`decimal-select ${this.decimals === 4 ? 'selected' : ''}`" @click="changeDecimals(4)">4</div>
    </div>
    <div class="settings-row-no-hover flex-row"></div>
    <app-form-field title="settings.select_display_currency">
      <select-list :options="currencyOptions" :default="this.currency" v-model="currencyLocal" />
    </app-form-field>
  </div>
</template>

<script>
import { mapState } from "vuex";
import UIConfig from "../../../ui-config.json";

export default {
  data() {
    return {
      isSingleAccount: UIConfig.isSingleAccount,
      themes: UIConfig.themes,
      currencyLocal: this.currency
    };
  },
  computed: {
    ...mapState("app", ["theme", "language", "decimals", "currency", "currencies"]),
    hasThemes() {
      return this.themes && this.themes.length;
    },
    currencyOptions() {
      var currencyArray = [];
      this.currencies.forEach(item => {
        currencyArray.push({
          value: item.code,
          label: item.name,
          rate: item.rate
        });
      });
      return currencyArray;
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
    changeCurrency(currency) {
      this.$store.dispatch("app/SET_CURRENCY", currency);
      this.$store.dispatch("app/SET_RATE", currency.rate);

      this.$forceUpdate();
    },
    routeTo(route) {
      this.$router.push({ name: route });
    }
  },
  watch: {
    currencyLocal(newValue, oldValue) {
      const symbolArray = [
        { value: "Eur", symbol: "€" },
        { value: "BTC", symbol: "₿" },
        { value: "AUD", symbol: "A$" },
        { value: "BRL", symbol: "R$" },
        { value: "CNT", symbol: "CN¥" },
        { value: "GBP", symbol: "£" },
        { value: "RUB", symbol: "₽" },
        { value: "USD", symbol: "$" },
        { value: "ZAR", symbol: "R" },
        { value: "KRW", symbol: "₩" },
        { value: "JPY", symbol: "¥" }
      ];
      if (newValue.value !== oldValue.value) {
        newValue.symbol = symbolArray.find(item => item.value.toLowerCase() === newValue.value.toLowerCase()).symbol;
        this.changeCurrency(newValue);
      } else {
        this.currency.symbol = symbolArray.find(item => item.value.toLowerCase() === this.currency.value.toLowerCase()).symbol;
        this.changeCurrency(this.currency);
      }
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

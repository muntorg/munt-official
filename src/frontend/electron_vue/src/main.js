import Vue from "vue";
import App from "./App.vue";
import store from "./store";
import router from "./router";
import i18n from "./i18n";

import VueCurrencyInput from "vue-currency-input";
Vue.use(VueCurrencyInput);

import "./components";

import { FontAwesomeIcon } from "@fortawesome/vue-fontawesome";
import { library } from "@fortawesome/fontawesome-svg-core";
import {
  faCog,
  faCopy,
  faLongArrowLeft,
  faLongArrowRight
} from "@fortawesome/pro-light-svg-icons";

library.add([faCog, faCopy, faLongArrowLeft, faLongArrowRight]);

Vue.component("fa-icon", FontAwesomeIcon);

Vue.config.productionTip = false;

new Vue({
  store,
  router,
  i18n,
  render: h => h(App)
}).$mount("#app");

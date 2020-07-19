import Vue from "vue";
import App from "./App.vue";
import store from "./store";
import router from "./router";
import i18n from "./i18n";

import "./components";
import "./layouts";

import { FontAwesomeIcon } from "@fortawesome/vue-fontawesome";
import { library } from "@fortawesome/fontawesome-svg-core";
import {
  faAngleDoubleLeft,
  faChevronDown,
  faChevronRight,
  faCog,
  faCopy,
  faCreditCard,
  faLongArrowLeft,
  faLongArrowRight,
  faPlus,
  faTimes,
  faUniversity
} from "@fortawesome/pro-light-svg-icons";

library.add([
  faAngleDoubleLeft,
  faChevronDown,
  faChevronRight,
  faCog,
  faCopy,
  faCreditCard,
  faLongArrowLeft,
  faLongArrowRight,
  faPlus,
  faTimes,
  faUniversity
]);

Vue.component("fa-icon", FontAwesomeIcon);

Vue.config.productionTip = false;

new Vue({
  store,
  router,
  i18n,
  render: h => h(App)
}).$mount("#app");

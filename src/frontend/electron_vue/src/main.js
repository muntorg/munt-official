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
  faArrowFromBottom,
  faArrowLeft,
  faArrowToBottom,
  faBan,
  faChevronDown,
  faChevronRight,
  faCog,
  faCopy,
  faCreditCard,
  faGem,
  faHourglassStart,
  faLock,
  faLongArrowLeft,
  faLongArrowRight,
  faPlus,
  faShield,
  faShieldCheck,
  faTimes,
  faUniversity,
  faUnlock,
  faUserCircle
} from "@fortawesome/pro-light-svg-icons";

library.add([
  faAngleDoubleLeft,
  faArrowFromBottom,
  faArrowLeft,
  faArrowToBottom,
  faBan,
  faChevronDown,
  faChevronRight,
  faCog,
  faCopy,
  faCreditCard,
  faGem,
  faHourglassStart,
  faLock,
  faLongArrowLeft,
  faLongArrowRight,
  faPlus,
  faShield,
  faShieldCheck,
  faTimes,
  faUniversity,
  faUnlock,
  faUserCircle
]);

Vue.component("fa-icon", FontAwesomeIcon);

Vue.config.productionTip = false;

new Vue({
  store,
  router,
  i18n,
  render: h => h(App)
}).$mount("#app");

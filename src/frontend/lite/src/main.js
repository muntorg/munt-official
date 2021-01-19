import Vue from "vue";
import App from "./App.vue";
import store from "./store";
import router from "./router";
import i18n from "./i18n";

import "./components";

import VueSlider from "vue-slider-component";
import "vue-slider-component/theme/antd.css";
Vue.component("VueSlider", VueSlider);

import ToggleButton from "vue-js-toggle-button";
Vue.use(ToggleButton);

import PortalVue from "portal-vue";
Vue.use(PortalVue);

import { FontAwesomeIcon } from "@fortawesome/vue-fontawesome";
import { library } from "@fortawesome/fontawesome-svg-core";
import {
  faAngleDoubleLeft,
  faArrowLeft,
  faBan,
  faChevronDown,
  faChevronRight,
  faCog,
  faCopy,
  faCreditCard,
  faEraser,
  faGem,
  faHourglassStart,
  faListUl,
  faLock,
  faLongArrowLeft,
  faLongArrowRight,
  faPlus,
  faSearchMinus,
  faSearchPlus,
  faShield,
  faShieldCheck,
  faTimes,
  faUniversity,
  faUnlock,
  faRedoAlt
} from "@fortawesome/pro-light-svg-icons";

import {
  faArrowCircleDown,
  faArrowCircleUp
} from "@fortawesome/free-solid-svg-icons";

library.add([
  faAngleDoubleLeft,
  faArrowLeft,
  faArrowCircleDown,
  faArrowCircleUp,
  faBan,
  faChevronDown,
  faChevronRight,
  faCog,
  faCopy,
  faCreditCard,
  faEraser,
  faGem,
  faHourglassStart,
  faListUl,
  faLock,
  faLongArrowLeft,
  faLongArrowRight,
  faPlus,
  faSearchMinus,
  faSearchPlus,
  faShield,
  faShieldCheck,
  faTimes,
  faUniversity,
  faUnlock,
  faRedoAlt
]);

Vue.component("fa-icon", FontAwesomeIcon);

Vue.config.productionTip = false;

new Vue({
  store,
  router,
  i18n,
  render: h => h(App)
}).$mount("#app");

import Vue from "vue";

import CurrencyInput from "./CurrencyInput";
import ClipboardField from "./ClipboardField";
import MainHeader from "./layout/MainHeader";
import AppButtonSection from "./layout/AppButtonSection";
import AppFormField from "./layout/AppFormField";
import AppSection from "./layout/AppSection";
import SelectList from "./SelectList";
import FooterButton from "./layout/FooterButton";
import ContentWrapper from "./layout/ContentWrapper";
import AccountHeader from "./AccountHeader";

Vue.component(ClipboardField.name, ClipboardField);
Vue.component(CurrencyInput.name, CurrencyInput);
Vue.component(MainHeader.name, MainHeader);
Vue.component(AppButtonSection.name, AppButtonSection);
Vue.component(AppFormField.name, AppFormField);
Vue.component(AppSection.name, AppSection);
Vue.component(SelectList.name, SelectList);
Vue.component(FooterButton.name, FooterButton);
Vue.component(ContentWrapper.name, ContentWrapper);
Vue.component(AccountHeader.name, AccountHeader);

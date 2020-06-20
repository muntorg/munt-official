import Vue from "vue";

import PhraseRepeatInput from "./PhraseRepeatInput";
import AppButton from "./AppButton";
import AppInput from "./AppInput";

Vue.component(PhraseRepeatInput.name, PhraseRepeatInput);
Vue.component("novo-button", AppButton);
Vue.component("novo-input", AppInput);

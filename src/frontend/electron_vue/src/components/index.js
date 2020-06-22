import Vue from "vue";

import AppButton from "./AppButton";
import AppInput from "./AppInput";
import PhraseValidator from "./PhraseValidator";

Vue.component(`Novo${AppButton.name.replace("App", "")}`, AppButton);
Vue.component(`Novo${AppInput.name.replace("App", "")}`, AppInput);
Vue.component(`Novo${PhraseValidator.name}`, PhraseValidator);

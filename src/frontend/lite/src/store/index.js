import Vue from "vue";
import Vuex from "vuex";
import { createPersistedState, createSharedMutations } from "vuex-electron";
import syncState from "./syncState";
import Store from "electron-store";
import walletPath from "../walletPath";

import modules from "./modules";

Vue.use(Vuex);

let userSettingsPersistedState = () => {};
// create persistate state in main process only
if (process.type !== "renderer") {
  // use user-settings.json file to store settings in wallet folder
  let store = new Store({ cwd: walletPath, name: "user-settings" });

  userSettingsPersistedState = createPersistedState({
    storage: store,
    invertIgnored: true, // use invertIgnored because we only want to store the paths in the ignoredPaths list and only persist on specific commits
    ignoredPaths: [
      // only store the paths listed below
      "app.theme",
      "app.language",
      "app.decimals"
    ],
    ignoredCommits: [
      // only update persisted state on commits listed below
      "app/SET_THEME",
      "app/SET_LANGUAGE",
      "app/SET_DECIMALS"
    ]
  });
}
export default new Vuex.Store({
  modules: modules,
  plugins: [syncState, userSettingsPersistedState, createSharedMutations()]
});

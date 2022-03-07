import AppStatus from "../../AppStatus";
import { Menu } from "electron";

function EnableDebugWindowOnCoreReady() {
  try {
    let menu = Menu.getApplicationMenu();
    if (menu === null) return;
    menu.items.find(x => x.label === "Help").submenu.items.find(x => x.label === "Debug window").enabled = true;
  } catch (e) {
    console.error(e);
  }
}

const app = {
  namespaced: true,
  state: {
    coreReady: false,
    syncDone: false,
    activityIndicator: false,
    splashReady: false,
    status: AppStatus.start,
    unityVersion: null,
    walletExists: null,
    walletVersion: null,
    rate: null,
    language: "en",
    decimals: 2
  },
  mutations: {
    SET_CORE_READY(state) {
      state.coreReady = true;
    },
    SET_SYNC_DONE(state) {
      state.syncDone = true;
    },
    SET_SPLASH_READY(state) {
      state.splashReady = true;
    },
    SET_STATUS(state, status) {
      if (state.status === AppStatus.shutdown) return; // shutdown in progress, do not switch to other status
      state.status = status;
    },
    SET_UNITY_VERSION(state, version) {
      state.unityVersion = version;
    },
    SET_RATE(state, rate) {
      state.rate = rate;
    },
    SET_WALLET_EXISTS(state, walletExists) {
      state.walletExists = walletExists;
    },
    SET_WALLET_VERSION(state, version) {
      state.walletVersion = version;
    },
    SET_ACTIVITY_INDICATOR(state, activityIndicator) {
      state.activityIndicator = activityIndicator;
    },
    SET_LANGUAGE(state, language) {
      state.language = language;
    },
    SET_DECIMALS(state, decimal) {
      state.decimals = decimal;
    }
  },
  actions: {
    SET_CORE_READY({ commit, state }) {
      commit("SET_STATUS", state.progress === 1 ? AppStatus.ready : AppStatus.synchronize);
      commit("SET_CORE_READY");
      EnableDebugWindowOnCoreReady();
    },
    SET_SYNC_DONE({ commit }) {
      commit("SET_SYNC_DONE");
    },
    SET_SPLASH_READY({ commit }) {
      commit("SET_SPLASH_READY");
    },
    SET_STATUS({ commit }, status) {
      commit("SET_STATUS", status);
    },
    SET_UNITY_VERSION({ commit }, version) {
      commit("SET_UNITY_VERSION", version);
    },
    SET_RATE({ commit }, rate) {
      commit("SET_RATE", rate);
    },
    SET_ACTIVITY_INDICATOR({ commit }, activityIndicator) {
      commit("SET_ACTIVITY_INDICATOR", activityIndicator);
    },
    SET_LANGUAGE({ commit }, language) {
      commit("SET_LANGUAGE", language);
    },
    SET_DECIMALS({ commit }, decimal) {
      commit("SET_DECIMALS", decimal);
    },
    SET_WALLET_EXISTS({ commit }, walletExists) {
      let status = walletExists ? AppStatus.synchronize : AppStatus.setup;
      commit("SET_STATUS", status);
      commit("SET_WALLET_EXISTS", walletExists);
    },
    SET_WALLET_VERSION({ commit }, version) {
      commit("SET_WALLET_VERSION", version);
    }
  }
};

export default app;

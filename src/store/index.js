import Vue from "vue";
import Vuex from "vuex";

import { app } from "electron";

import { createPersistedState, createSharedMutations } from "vuex-electron";
import Store from "electron-store";

let store = new Store();
if (process.type !== "renderer") {
  console.log(`clear store on start`);
  store.clear();
  app.on("quit", () => {
    console.log(`clear store on quit`);
    store.clear();
  });
}

Vue.use(Vuex);

export const AppStatus = {
  start: "start",
  setup: "setup",
  synchronize: "synchronize",
  ready: "ready",
  shutdown: "shutdown"
};

export default new Vuex.Store({
  state: {
    status: AppStatus.start,
    coreReady: false
  },
  mutations: {
    SET_BALANCE(state, payload) {
      state.balance = payload.balance;
    },
    SET_CORE_READY(state, payload) {
      state.coreReady = payload.coreReady;
    },
    SET_MUTATIONS(state, payload) {
      state.mutations = payload.mutations;
    },
    SET_PRICE_INFO(state, payload) {
      state.priceInfo = payload.priceInfo;
    },
    SET_RECEIVE_ADDRESS(state, payload) {
      state.receiveAddress = payload.receiveAddress;
    },
    SET_STATUS(state, payload) {
      state.status = payload.status;
    },
    SET_UNITY_VERSION(state, payload) {
      state.unityVersion = payload.version;
    },
    SET_WALLET_VERSION(state, payload) {
      state.walletVersion = payload.version;
    }
  },
  actions: {
    SET_BALANCE({ commit }, payLoad) {
      commit(payLoad);
    },
    SET_CORE_READY({ commit }, payLoad) {
      commit(payLoad);
    },
    SET_GULDEN_VERSION({ commit }, payLoad) {
      commit(payLoad);
    },
    SET_MUTATIONS({ commit }, payLoad) {
      commit(payLoad);
    },
    SET_PRICE_INFO({ commit }, payLoad) {
      commit(payLoad);
    },
    SET_RECEIVE_ADDRESS({ commit }, payLoad) {
      commit(payLoad);
    },
    SET_STATUS({ commit }, payLoad) {
      commit(payLoad);
    },
    SET_WALLET_EXISTS({ commit }, payLoad) {
      commit(payLoad);
    },
    SET_UNITY_VERSION({ commit }, payload) {
      commit(payload);
    },
    SET_WALLET_VERSION({ commit }, payload) {
      commit(payload);
    }
  },
  modules: {},
  plugins: [createPersistedState({ storage: store }), createSharedMutations()]
});

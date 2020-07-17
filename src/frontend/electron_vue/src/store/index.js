import Vue from "vue";
import Vuex from "vuex";
import { createSharedMutations } from "vuex-electron";
import syncState from "./syncState";
import cloneDeep from "lodash.clonedeep";

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
    balance: null,
    coreReady: false,
    mutations: null,
    priceInfo: null,
    receiveAddress: null,
    status: AppStatus.start,
    unityVersion: null,
    walletExists: null,
    walletVersion: null
  },
  mutations: {
    REPLACE_STATE(state, payload) {
      this.replaceState(cloneDeep(payload.state));
    },
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
    SET_STATUS(state, status) {
      if (state.status === AppStatus.shutdown) return;
      if (status === AppStatus.synchronize) {
        console.log(`skip synchronization for now`);
        status = AppStatus.ready;
      }
      state.status = status;
    },
    SET_UNITY_VERSION(state, payload) {
      state.unityVersion = payload.version;
    },
    SET_WALLET_EXISTS(state, walletExists) {
      state.walletExists = walletExists;
    },
    SET_WALLET_VERSION(state, payload) {
      state.walletVersion = payload.version;
    }
  },
  actions: {
    REPLACE_STATE({ commit }, payload) {
      commit(payload);
    },
    SET_BALANCE({ commit }, payload) {
      commit(payload);
    },
    SET_CORE_READY({ commit }, payload) {
      commit(payload);
    },
    SET_GULDEN_VERSION({ commit }, payload) {
      commit(payload);
    },
    SET_MUTATIONS({ commit }, payload) {
      commit(payload);
    },
    SET_PRICE_INFO({ commit }, payload) {
      commit(payload);
    },
    SET_RECEIVE_ADDRESS({ commit }, payload) {
      commit(payload);
    },
    SET_STATUS({ commit }, payload) {
      commit("SET_STATUS", payload.status);
    },
    SET_UNITY_VERSION({ commit }, payload) {
      commit(payload);
    },
    SET_WALLET_EXISTS({ commit }, payload) {
      let status = payload.walletExists
        ? AppStatus.synchronize
        : AppStatus.setup;
      commit("SET_STATUS", status);
      commit("SET_WALLET_EXISTS", payload.walletExists);
    },
    SET_WALLET_VERSION({ commit }, payload) {
      commit(payload);
    }
  },
  getters: {
    totalBalance: state => {
      let balance = state.balance;
      if (balance === undefined || balance === null) return null;
      return (
        (balance.availableIncludingLocked +
          balance.unconfirmedIncludingLocked +
          balance.immatureIncludingLocked) /
        100000000
      ).toFixed(2);
    }
  },
  plugins: [syncState, createSharedMutations()]
});

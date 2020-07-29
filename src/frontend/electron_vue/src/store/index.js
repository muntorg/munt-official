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
    accounts: [],
    activeAccount: null,
    balance: null,
    coreReady: false,
    generationActive: false,
    generationStats: null,
    mutations: null,
    receiveAddress: null,
    status: AppStatus.start,
    unityVersion: null,
    walletBalance: null,
    walletExists: null,
    walletPassword: null,
    walletVersion: null
  },
  mutations: {
    REPLACE_STATE(state, payload) {
      this.replaceState(cloneDeep(payload.state));
    },
    SET_ACCOUNTS(state, payload) {
      state.accounts = payload.accounts;
    },
    SET_ACTIVE_ACCOUNT(state, payload) {
      state.activeAccount = payload.accountUUID;
    },
    SET_BALANCE(state, payload) {
      state.balance = payload.balance;
    },
    SET_CORE_READY(state, payload) {
      state.coreReady = payload.coreReady;
    },
    SET_GENERATION_ACTIVE(state, payload) {
      state.generationActive = payload.generationActive;
    },
    SET_GENERATION_STATS(state, payload) {
      state.generationStats = payload.generationStats;
    },
    SET_MUTATIONS(state, payload) {
      state.mutations = payload.mutations;
    },
    SET_RECEIVE_ADDRESS(state, payload) {
      state.receiveAddress = payload.receiveAddress;
    },
    SET_STATUS(state, status) {
      if (state.status === AppStatus.shutdown) return; // shutdown in progress, do not switch to other status
      state.status = status;
    },
    SET_UNITY_VERSION(state, payload) {
      state.unityVersion = payload.version;
    },
    SET_WALLET_BALANCE(state, payload) {
      state.walletBalance = payload.walletBalance;
    },
    SET_WALLET_EXISTS(state, walletExists) {
      state.walletExists = walletExists;
    },
    SET_WALLET_PASSWORD(state, payload) {
      state.walletPassword = payload.walletPassword;
    },
    SET_WALLET_VERSION(state, payload) {
      state.walletVersion = payload.version;
    }
  },
  actions: {
    REPLACE_STATE({ commit }, payload) {
      commit(payload);
    },
    SET_ACCOUNT_NAME({ state, commit }, payload) {
      let accounts = [...state.accounts];
      let account = accounts.find(x => x.UUID === payload.accountUUID);
      account.label = payload.newAccountName;
      commit({ type: "SET_ACCOUNTS", accounts: accounts });
    },
    SET_ACCOUNTS({ commit }, payload) {
      commit(payload);
    },
    SET_ACTIVE_ACCOUNT({ commit }, payload) {
      commit(payload);
    },
    SET_BALANCE({ commit }, payload) {
      commit(payload);
    },
    SET_CORE_READY({ commit }, payload) {
      commit("SET_STATUS", AppStatus.ready); // set status to ready. maybe core_ready is redundant and can be removed.
      commit(payload);
    },
    SET_GULDEN_VERSION({ commit }, payload) {
      commit(payload);
    },
    SET_GENERATION_ACTIVE({ commit }, payload) {
      commit(payload);
    },
    SET_GENERATION_STATS({ commit }, payload) {
      commit(payload);
    },
    SET_MUTATIONS({ commit }, payload) {
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
    SET_WALLET_BALANCE({ commit }, payload) {
      commit(payload);
    },
    SET_WALLET_EXISTS({ commit }, payload) {
      let status = payload.walletExists
        ? AppStatus.synchronize
        : AppStatus.setup;
      commit("SET_STATUS", status);
      commit("SET_WALLET_EXISTS", payload.walletExists);
    },
    SET_WALLET_PASSWORD({ commit }, payload) {
      commit(payload);
    },
    SET_WALLET_VERSION({ commit }, payload) {
      commit(payload);
    }
  },
  getters: {
    totalBalance: state => {
      let balance = state.walletBalance;
      if (balance === undefined || balance === null) return null;
      return (
        (balance.availableIncludingLocked +
          balance.unconfirmedIncludingLocked +
          balance.immatureIncludingLocked) /
        100000000
      ).toFixed(2);
    },
    accounts: state => {
      return state.accounts
        .filter(x => x.state === "Normal")
        .sort((a, b) => {
          const labelA = a.label.toUpperCase();
          const labelB = b.label.toUpperCase();

          let comparison = 0;
          if (labelA > labelB) {
            comparison = 1;
          } else if (labelA < labelB) {
            comparison = -1;
          }
          return comparison;
        });
    },
    account: state => {
      return state.accounts.find(x => x.UUID === state.activeAccount);
    },
    miningAccount: state => {
      return state.accounts.find(
        x => x.type === "Mining" && x.state === "Normal"
      ); // this will retrieve the first account if type Mining
    }
  },
  plugins: [syncState, createSharedMutations()]
});

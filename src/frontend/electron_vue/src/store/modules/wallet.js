const wallet = {
  namespaced: true,
  state: {
    accounts: [],
    activeAccount: null,
    balance: null,
    mutations: null,
    receiveAddress: null,
    walletBalance: null,
    walletPassword: null
  },
  mutations: {
    SET_ACCOUNTS(state, accounts) {
      state.accounts = accounts;
    },
    SET_ACTIVE_ACCOUNT(state, accountUUID) {
      state.activeAccount = accountUUID;
    },
    SET_BALANCE(state, balance) {
      state.balance = balance;
    },
    SET_MUTATIONS(state, mutations) {
      state.mutations = mutations;
    },
    SET_RECEIVE_ADDRESS(state, receiveAddress) {
      state.receiveAddress = receiveAddress;
    },
    SET_WALLET_BALANCE(state, walletBalance) {
      state.walletBalance = walletBalance;
    },
    SET_WALLET_PASSWORD(state, password) {
      state.walletPassword = password;
    }
  },
  actions: {
    SET_ACCOUNT_NAME({ state, commit }, payload) {
      let accounts = [...state.accounts];
      let account = accounts.find(x => x.UUID === payload.accountUUID);
      account.label = payload.newAccountName;
      commit("SET_ACCOUNTS", accounts);
    },
    SET_ACCOUNTS({ commit }, accounts) {
      commit("SET_ACCOUNTS", accounts);
    },
    SET_ACTIVE_ACCOUNT({ commit }, accountUUID) {
      // clear mutations and receive address
      commit("SET_RECEIVE_ADDRESS", { receiveAddress: "" });
      commit("SET_ACTIVE_ACCOUNT", accountUUID);
      commit("SET_MUTATIONS", { mutations: null });
    },
    SET_BALANCE({ commit }, new_balance) {
      commit("SET_BALANCE", new_balance);
    },
    SET_MUTATIONS({ commit }, mutations) {
      commit("SET_MUTATIONS", mutations);
    },
    SET_RECEIVE_ADDRESS({ commit }, receiveAddress) {
      commit("SET_RECEIVE_ADDRESS", receiveAddress);
    },
    SET_WALLET_BALANCE({ commit }, walletBalance) {
      commit("SET_WALLET_BALANCE", walletBalance);
    },
    SET_WALLET_PASSWORD({ commit }, password) {
      commit("SET_WALLET_PASSWORD", password);
    }
  },
  getters: {
    totalBalance: state => {
      let balance = state.walletBalance;
      if (balance === undefined || balance === null) return null;
      return (
        (balance.availableIncludingLocked +
          balance.unconfirmedIncludingLocked +
          balance.immatureIncludingLocked)
      );
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
      ); // this will retrieve the first account of type Mining
    }
  }
};

export default wallet;

const mining = {
  namespaced: true,
  state: {
    active: false,
    stats: null,
    settings: {
      memorySize: null,
      threadCount: null,
      arenaThreadCount: null
    }
  },
  mutations: {
    SET_ACTIVE(state, active) {
      state.active = active;
    },
    SET_MEMORY_SIZE(state, memorySize) {
      state.settings.memorySize = memorySize;
    },
    SET_STATS(state, stats) {
      state.stats = stats;
    },
    SET_THREAD_COUNT(state, threadCount) {
      state.settings.threadCount = threadCount;
    },
    SET_ARENA_THREAD_COUNT(state, arenaThreadCount) {
      state.settings.arenaThreadCount = arenaThreadCount;
    }
  },
  actions: {
    SET_ACTIVE({ commit }, active) {
      commit("SET_ACTIVE", active);
    },
    SET_MEMORY_SIZE({ commit }, memorySize) {
      commit("SET_MEMORY_SIZE", memorySize);
    },
    SET_STATS({ commit }, stats) {
      commit("SET_STATS", stats);
    },
    SET_THREAD_COUNT({ commit }, threadCount) {
      commit("SET_THREAD_COUNT", threadCount);
    },
    SET_ARENA_THREAD_COUNT({ commit }, arenaThreadCount) {
      commit("SET_ARENA_THREAD_COUNT", arenaThreadCount);
    }
  }
};

export default mining;

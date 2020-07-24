<template>
  <div class="mutation-list">
    <div
      class="mutation-group"
      v-for="group in groupedMutations"
      :key="group.idx"
    >
      <h4>{{ formatDate(group.date) }}</h4>
      <div
        class="mutation-row"
        v-for="mutation in group.mutations"
        :key="mutation.txHash"
      >
        <div class="icon">
          <fa-icon :icon="['fal', mutationIcon(mutation)]" />
        </div>
        <div class="time">{{ formatTime(mutation.timestamp) }}</div>
        <div class="amount">{{ formatAmount(mutation.change) }}</div>
      </div>
    </div>
  </div>
</template>

<script>
export default {
  name: "MutationList",
  props: {
    mutations: null
  },
  computed: {
    groupedMutations() {
      if (this.mutations === null) return [];
      let groupedMutations = [];
      let currentGroup = null;

      for (let i = 0; i < this.mutations.length; i++) {
        let mutation = this.mutations[i];
        let date = new Date(mutation.timestamp * 1000);
        let dateStart = new Date(
          date.getFullYear(),
          date.getMonth(),
          date.getDate()
        );

        if (
          currentGroup === null ||
          currentGroup.date.toString() !== dateStart.toString()
        ) {
          currentGroup = {
            idx: groupedMutations.length,
            date: dateStart,
            mutations: []
          };

          groupedMutations.push(currentGroup);
        }
        currentGroup.mutations.push(mutation);
      }
      return groupedMutations;
    }
  },
  methods: {
    mutationIcon(mutation) {
      switch (mutation.status) {
        case 0: // UNCONFIRMED
          return "hourglass-start";
        case 1: // CONFIRMING
          return "shield";
        case 2: // CONFIRMED
          return "shield-check";
        case 3: // ABANDONED
        case 4: // CONFLICTED
          return "ban";
      }
    },
    formatDate(d) {
      let date = new Date(d);
      let options = {
        year: "numeric",
        month: "long",
        day: "numeric"
      };
      if (date.getFullYear() === new Date().getFullYear()) delete options.year;

      // for now determine localization here. replace by global method
      let language =
        process.env.VUE_APP_I18N_LOCALE ||
        window.navigator.language.slice(0, 2);
      switch (language) {
        case "nl":
        case "en":
          break;
        default:
          language = "en";
          break;
      }

      return date.toLocaleString(language, options);
    },
    formatTime(timestamp) {
      let date = new Date(timestamp * 1000);
      return `${("0" + date.getHours()).slice(-2)}:${(
        "0" + date.getMinutes()
      ).slice(-2)}`;
    },
    formatAmount(amount) {
      return `${(amount / 100000000).toFixed(2)}`;
    }
  }
};
</script>

<style lang="less" scoped>
.mutation-group {
  margin-bottom: 20px;
}

h4 {
  color: #999;
  margin-bottom: 10px;
}

.mutation-row {
  display: flex;
  flex-direction: row;
  margin-bottom: 4px;

  & > div {
    padding-right: 8px;
  }

  & .amount {
    flex: 1;
    text-align: right;
  }
}
</style>

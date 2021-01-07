<template>
  <div class="mutation-list">
    <div
      class="mutation-group"
      v-for="group in groupedMutations"
      :key="group.idx"
    >
      <h4>{{ formatDate(group.date) }}</h4>
      <div
        class="mutation-row flex-row"
        v-for="mutation in group.mutations"
        :key="mutation.txHash"
        @click="selectTxHash(mutation.txHash)"
        @contextmenu="showTxMenu($event, mutation)"
        :class="mutationRowClass(mutation.txHash)"
      >
        <div class="icon transactionicon">
          <fa-icon :icon="['fal', mutationIcon(mutation)]" />
        </div>
        <div class="time">{{ formatTime(mutation.timestamp) }}</div>
        <div class="amount">{{ formatAmount(mutation.change) }}</div>
      </div>
    </div>
  </div>
</template>

<script>
const { remote } = require("electron");
const { Menu, MenuItem } = remote;
import { WalletController } from "../../../unity/Controllers";
export default {
  name: "MutationList",
  props: {
    mutations: null,
    txHash: null
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
    },
    selectTxHash(txHash) {
      this.$emit("tx-hash", txHash);
    },
    showTxMenu(e, mutation) {
      if (
        mutation.status !== 1 &&
        mutation.status !== 2 &&
        mutation.status !== 3
      ) {
        const contextMenu = new Menu();
        contextMenu.append(
          new MenuItem({
            label: "Abandon transaction",
            click() {
              WalletController.AbandonTransaction(mutation.txHash);
            }
          })
        );
        contextMenu.popup({ x: e.x, y: e.y });
      }
    },
    mutationRowClass(txHash) {
      return txHash === this.txHash ? "selected" : "";
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
  width: calc(100% + 40px);
  margin: 0 0 0 -20px;
  padding: 5px 20px 5px 20px;
  cursor: pointer;

  & .amount {
    flex: 1;
    text-align: right;
  }

  &:hover {
    background: #f5f5f5;
  }

  &.selected {
    background: var(--primary-color);
    color: #fff;
  }
}
</style>

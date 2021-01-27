<template>
  <div class="transaction-details-dialog">
    <div class="tx-date">{{ computedTimestamp }}</div>
    <div class="tx-amount">{{ computedAmount }}</div>
    <div class="tx-to">
      <fa-icon :icon="['far', 'long-arrow-down']" />
    </div>
    <clipboard-field :value="mutation.recipient_addresses" />
    <h4>TX ID</h4>
    <div class="tx-id"><clipboard-field :value="mutation.txHash" /></div>
  </div>
</template>

<script>
import EventBus from "../EventBus";
import ClipboardField from "./ClipboardField.vue";

export default {
  components: { ClipboardField },
  name: "TransactionDetailsDialog",
  props: {
    mutation: {
      type: Object,
      default: null
    }
  },
  computed: {
    computedTimestamp() {
      let timestamp = new Date(this.mutation.timestamp * 1000);
      return `${this.formatDate(timestamp)} ${this.formatTime(timestamp)}`;
    },
    computedAmount() {
      return `${(this.mutation.change / 100000000).toFixed(2)} NLG`;
    }
  },
  methods: {
    close() {
      EventBus.$emit("close-dialog");
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
    }
  }
};
</script>

<style lang="less" scoped>
.transaction-details-dialog {
  text-align: center;

  & > h4 {
    margin: 20px 0 0 0;
  }
}
.tx-date {
  margin: 0 0 10px 0;
  font-size: .9em;
}
.tx-amount {
  font-size: 1.6em;
  font-weight: 600;
}
.tx-to {
  margin: 15px 0 15px 0;
  font-size: 1.6em;
}
.tx-id {
  padding: 10px;
  font-weight: 500;
  font-size: .8em;
  text-transform: uppercase;
}
</style>

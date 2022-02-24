<template>
  <div class="transaction-details-dialog">
    <div class="tx-date">{{ computedTimestamp }}</div>
    <div class="tx-amount">{{ computedAmount }}</div>
    <div class="tx-to">
      <fa-icon :icon="['far', 'long-arrow-down']" />
    </div>
    <div class="tx-address">
      <clipboard-field :value="mutation.recipient_addresses" />
    </div>
    <div class="tx-id">TX ID:<clipboard-field :value="mutation.txHash" /></div>
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
      return `${(this.mutation.change / 100000000).toFixed(2)} XFL`;
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
      return date.toLocaleString(this.$i18n.locale, options);
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
    margin: 25px 0 0 0;
  }
}
.tx-date {
  margin: 0 0 10px 0;
  font-size: 0.9em;
}
.tx-amount {
  font-size: 1.6em;
  font-weight: 600;
}
.tx-address {
  font-weight: 500;
}
.tx-to {
  margin: 20px 0 10px 0;
  font-size: 1.6em;
}
.tx-id {
  margin: 20px 0 0 0;
  padding: 10px;
  font-size: 0.75em;
  line-height: 1em;
  text-transform: uppercase;
  & .clipboard-field {
    display: inline-block;
    margin: 0 0 0 5px;
  }
}
</style>

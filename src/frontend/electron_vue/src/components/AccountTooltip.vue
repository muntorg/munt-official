<template>
  <div class="tooltip" @mouseleave="hideTooltip">
    <div @mouseenter="showTooltip">
      <slot></slot>
    </div>
    <div @mouseenter="showTooltip" class="tooltip-container" v-if="show">
      <div class="tooltip-heading" v-if="type == 'Account'">{{ $t("tooltip.account_balance") }}</div>
      <div class="tooltip-heading" v-else>{{ $t("tooltip.wallet_balance") }}</div>
      <div>
        <div class="tooltip-row">
          <div class="tooltip-content" style="flex: 1">{{ $t("tooltip.total") }}</div>
          <div class="tooltip-content">{{ accountObject.total }}</div>
        </div>
        <div v-if="account.type === 'Holding' || account.type === 'Witness' || type === 'Wallet'" class="tooltip-row">
          <div class="tooltip-content" style="flex: 1">{{ $t("tooltip.locked") }}</div>
          <div class="tooltip-content">{{ accountObject.locked }}</div>
        </div>
        <div class="tooltip-row">
          <div class="tooltip-content" style="flex: 1">{{ $t("tooltip.spendable") }}</div>
          <div class="tooltip-content">{{ accountObject.spendable }}</div>
        </div>
        <div class="tooltip-row">
          <div class="tooltip-content" style="flex: 1">{{ $t("tooltip.pending") }}</div>
          <div class="tooltip-content">{{ accountObject.pending }}</div>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import { mapGetters } from "vuex";
import { formatMoneyForDisplay } from "../util.js";

export default {
  name: "AccountTooltip",
  data() {
    return {
      show: false,
      accountObject: {},
      timeout: undefined
    };
  },
  computed: {
    ...mapGetters("wallet", ["totalBalance", "lockedBalance", "spendableBalance", "pendingBalance", "immatureBalance"])
  },
  props: {
    account: {},
    type: {
      type: String
    }
  },
  methods: {
    showTooltip: function () {
      clearTimeout(this.timeout);
      this.show = true;
      this.getValues();
    },
    hideTooltip() {
      clearTimeout(this.timeout);
      this.timeout = setTimeout(() => {
        this.show = false;
      }, 100);
    },
    getValues() {
      if (this.type === "Account") {
        const locked = this.account.allBalances.totalLocked || 0;
        const spendable = this.account.allBalances.availableExcludingLocked || 0;
        const pending = this.account.allBalances.unconfirmedExcludingLocked || 0;
        const immature = this.account.allBalances.immatureExcludingLocked || 0;

        this.accountObject = {
          locked: formatMoneyForDisplay(locked),
          spendable: formatMoneyForDisplay(spendable),
          pending: formatMoneyForDisplay(pending),
          immature: formatMoneyForDisplay(immature),
          total: formatMoneyForDisplay(locked + spendable + pending + immature)
        };
      } else {
        const locked = this.lockedBalance || 0;
        const spendable = this.spendableBalance || 0;
        const pending = this.pendingBalance || 0;
        const immature = this.immatureBalance || 0;

        this.accountObject = {
          locked: formatMoneyForDisplay(locked),
          spendable: formatMoneyForDisplay(spendable),
          pending: formatMoneyForDisplay(pending),
          immature: formatMoneyForDisplay(immature),
          total: formatMoneyForDisplay(locked + spendable + pending + immature)
        };
      }
    }
  },
  mounted() {
    this.getValues();
  }
};
</script>

<style lang="less" scoped>
.tooltip {
  position: relative;
  z-index: 99;
}
.tooltip-container {
  position: fixed;
  z-index: 9990;
  display: flex;
  flex-direction: column;
  margin-top: 6px;
  padding: 5px;
  border-radius: 2px;
  background-color: #fff;
  width: 200px;
  box-shadow: rgba(0, 0, 0, 0.24) 0px 3px 8px;
}
.tooltip-heading {
  font-size: 1em;
  margin-right: 10px;
  font-weight: bold;
  margin-bottom: 12px;
  color: #000;
  line-height: 22px;
}
.tooltip-content {
  font-size: 0.85em;
  color: #000;
  line-height: 22px;
}
.tooltip-row {
  display: flex;
  flex-direction: row;
  width: 100%;
  margin-right: 10px;
}
</style>

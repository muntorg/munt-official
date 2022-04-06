<template>
  <div @mouseenter="showTooltip" @mouseleave="showTooltip" class="tooltip">
    <slot></slot>
    <div v-if="show">
      <div class="tooltip-container">
        <div class="tooltip-heading" v-if="type === 'Account'">Account Balances</div>
        <div class="tooltip-heading" v-else>Wallet Balances</div>
        <div>
          <div class="tooltip-row">
            <div class="tooltip-content" style="flex: 1">Total</div>
            <div class="tooltip-content">{{ accountObject.total }}</div>
          </div>
          <div v-if="account.type === 'Holding' || account.type === 'Witness' || type === 'Wallet'" class="tooltip-row">
            <div class="tooltip-content" style="flex: 1">Locked</div>
            <div class="tooltip-content">{{ accountObject.locked }}</div>
          </div>
          <div class="tooltip-row">
            <div class="tooltip-content" style="flex: 1">Spendable</div>
            <div class="tooltip-content">{{ accountObject.spendable }}</div>
          </div>
          <div class="tooltip-row">
            <div class="tooltip-content" style="flex: 1">Pending</div>
            <div class="tooltip-content">{{ accountObject.pending }}</div>
          </div>
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
      accountObject: {}
    };
  },
  computed: {
    ...mapGetters("wallet", ["totalBalance", "lockedBalance", "spendableBalance", "pendingBalance", "immatureBalance"])
  },
  props: {
    account: {
      type: Object
    },
    type: {
      type: String
    }
  },
  methods: {
    showTooltip: function() {
      this.show = !this.show;
      if (this.show) {
        this.getValues();
      }
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
}
.tooltip-container {
  display: flex;
  flex-direction: column;
  margin-top: 6fpx;
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
}
.tooltip-content {
  font-size: 0.85em;
  color: #000;
}
.tooltip-row {
  display: flex;
  flex-direction: row;
  width: 100%;
  margin-right: 10px;
}
</style>

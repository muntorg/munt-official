<template>
  <div class="send-view flex-col">
    <div class="main">
      <div class="flex flex-row">
        <input
          helper="TEST"
          :class="getSendClass()"
          v-model="amount"
          ref="amount"
          type="number"
          step="0.00000001"
          :placeholder="computedAmountPlaceholder"
          min="0"
        />
        <button v-if="maxAmount > 0" outlined class="max" @click="setUseMax" :disabled="useMax">max</button>
      </div>
      <content-wrapper>
        <p>
          {{ this.useMax ? $t("send_coins.fee_will_be_subtracted") : "&nbsp;" }}
        </p>
      </content-wrapper>
      <div class="flex-row">
        <div @click="selectedTab = 'address'" :class="selectedTab === 'address' ? 'selected-tab' : 'unselected-tab'">
          <h4>Receiving Address</h4>
        </div>
        <div @click="selectedTab = 'accounts'" :class="selectedTab === 'accounts' ? 'selected-tab' : 'unselected-tab'">
          <h4>My Accounts</h4>
        </div>
      </div>
      <div v-if="selectedTab === 'address'">
        <input v-model="address" type="text" :placeholder="$t('send_coins.enter_coins_address')" :class="getAddressClass()" />
        <input v-model="label" type="text" :placeholder="$t('send_coins.enter_label')" />
      </div>
      <div v-else>
        <app-form-field title="send_coins.select_account">
          <select-list :options="accountsList" :default="accountsList[0]" v-model="selectedAccount" />
        </app-form-field>
      </div>
    </div>
    <div class="flex-row">
      <button class="send" @click="showConfirmation" :disabled="disableSendButton">
        {{ $t("buttons.send") }}
      </button>
      <button class="clear" outlined @click="clearInputs" :disabled="disableClearButton">{{ $t("buttons.clear") }}</button>
    </div>
  </div>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import { displayToMonetary, formatMoneyForDisplay } from "../../../util.js";
import { LibraryController, AccountsController } from "@/unity/Controllers";
import ConfirmTransactionDialog from "./ConfirmTransactionDialog";
import EventBus from "@/EventBus";

export default {
  name: "Send",
  data() {
    return {
      amount: "",
      maxAmount: null,
      address: "",
      label: "",
      isAddressInvalid: false,
      sellDisabled: false,
      useMax: false,
      selectedTab: "address",
      accountsList: [],
      selectedAccount: []
    };
  },
  computed: {
    ...mapState("app", ["decimals"]),
    ...mapGetters("wallet", ["account", "accounts"]),
    computedAmountPlaceholder() {
      return `0.${"0".repeat(this.decimals)}`;
    },
    computedMaxForDisplay() {
      return formatMoneyForDisplay(this.maxAmount, false, 8);
    },
    addressClass() {
      return this.isAddressInvalid ? "error" : "";
    },
    hasErrors() {
      return this.isAddressInvalid;
    },
    disableSendButton() {
      if (isNaN(parseFloat(this.amount))) return true;
      if (this.address === null || this.address.trim().length === 0) return true;
      if (this.amount > this.account.balance) return true;
      if (!LibraryController.IsValidNativeAddress(this.address)) return true;
      return false;
    },
    disableClearButton() {
      return this.amount === "" && this.address === "" && this.label === "";
    }
  },
  mounted() {
    this.$refs.amount.focus();
    this.accountsList = this.accounts.filter(item => item.UUID !== this.account.UUID);
    EventBus.$on("transaction-succeeded", this.onTransactionSucceeded);
  },
  beforeDestroy() {
    EventBus.$off("transaction-succeeded", this.onTransactionSucceeded);
  },
  watch: {
    account: {
      immediate: true,
      handler() {
        this.maxAmount = this.account.spendable;
      }
    },
    selectedAccount(value) {
      this.address = AccountsController.GetReceiveAddress(value.UUID);
      this.label = value.label;
    },
    maxAmount() {
      if (this.useMax) {
        this.amount = this.computedMaxForDisplay;
      }
    },
    amount() {
      // this will prevent entering more than 8 decimals
      const idx = this.amount.indexOf(".");
      if (idx !== -1 && idx + 9 < this.amount.length) {
        this.amount = this.amount.slice(0, idx + 9);
      }

      if (displayToMonetary(this.amount) >= this.maxAmount) {
        this.useMax = true;
      } else {
        this.useMax = false;
      }
    },
    useMax() {
      if (this.useMax) {
        this.amount = this.computedMaxForDisplay;
      }
    }
  },
  methods: {
    clearInputs() {
      this.amount = "";
      this.address = "";
      this.label = "";
      this.$refs.amount.focus();
    },
    getSendClass() {
      if (this.amount > this.account.balance) return "error";
      return "";
    },
    getAddressClass() {
      if (!LibraryController.IsValidNativeAddress(this.address)) return "error";
      return "";
    },
    showConfirmation() {
      // amount is always less then or equal to the floored spendable amount
      // if useMax is checked, use the maxAmount and subtract the fee from the amount
      let amount = this.useMax ? this.maxAmount : displayToMonetary(this.amount);

      // validate address
      this.isAddressInvalid = !LibraryController.IsValidNativeAddress(this.address);

      if (this.hasErrors) return;

      EventBus.$emit("show-dialog", {
        title: this.$t("send_coins.confirm_transaction"),
        component: ConfirmTransactionDialog,
        componentProps: {
          amount: amount,
          address: this.address,
          subtractFee: this.useMax
        },
        showButtons: false
      });
    },
    onTransactionSucceeded() {
      this.$router.push({ name: "transactions" });
    },
    setUseMax() {
      this.useMax = true;
    }
  }
};
</script>

<style lang="less" scoped>
.send-view {
  height: 100%;
  flex: 1;

  .main {
    flex: 1;
  }
}
.selected-tab {
  border-bottom: 2px solid #000000;
  margin: 0px 10px 10px 10px;
}
.unselected-tab {
  margin: 0px 10px 10px 10px;
}
.account-name {
  font-size: 0.85em;
  width: 100%;
  margin-right: 10px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.accounts-list {
  padding-left: 10px;
  height: 50%;
  overflow: scroll;
}

button.max,
button.clear {
  margin-left: 10px;
  padding: 0 15px;
}

button.send {
  width: 100%;
}
</style>

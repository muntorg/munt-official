<template>
  <div class="send-view flex-col">
    <div class="main">
      <div class="flex flex-row">
        <input v-model="amount" ref="amount" type="number" step="0.00000001" :placeholder="computedAmountPlaceholder" min="0" />
        <button v-if="maxAmount > 0" outlined class="max" @click="setUseMax" :disabled="useMax">max</button>
      </div>
      <content-wrapper>
        <p>
          {{ this.useMax ? $t("send_coins.fee_will_be_subtracted") : "&nbsp;" }}
        </p>
      </content-wrapper>
      <input v-model="address" type="text" :placeholder="$t('send_coins.enter_coins_address')" :class="addressClass" @keydown="isAddressInvalid = false" />
      <input v-model="label" type="text" :placeholder="$t('send_coins.enter_label')" />
      <input
        v-model="password"
        type="password"
        v-show="walletPassword === null"
        :placeholder="$t('common.enter_your_password')"
        :class="passwordClass"
        @keydown="onPasswordKeydown"
      />
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
import { LibraryController } from "@/unity/Controllers";
import ConfirmTransactionDialog from "./ConfirmTransactionDialog";
import EventBus from "@/EventBus";
import { BackendUtilities } from "@/unity/Controllers";

export default {
  name: "Send",
  data() {
    return {
      amount: "",
      maxAmount: null,
      address: "",
      label: "",
      password: "",
      isAddressInvalid: false,
      isPasswordInvalid: false,
      sellDisabled: false,
      useMax: false
    };
  },
  computed: {
    ...mapState("wallet", ["walletPassword"]),
    ...mapState("app", ["decimals"]),
    ...mapGetters("wallet", ["account"]),
    computedPassword() {
      return this.walletPassword ? this.walletPassword : this.password || "";
    },
    computedAmountPlaceholder() {
      return `0.${"0".repeat(this.decimals)}`;
    },
    computedMaxForDisplay() {
      return formatMoneyForDisplay(this.maxAmount, false, 8);
    },
    addressClass() {
      return this.isAddressInvalid ? "error" : "";
    },
    passwordClass() {
      return this.isPasswordInvalid ? "error" : "";
    },
    hasErrors() {
      return this.isAddressInvalid || this.isPasswordInvalid;
    },
    disableSendButton() {
      if (isNaN(parseFloat(this.amount))) return true;
      if (this.address === null || this.address.trim().length === 0) return true;
      if (this.computedPassword.trim().length === 0) return true;
      return false;
    },
    disableClearButton() {
      return this.amount === "" && this.address === "" && this.label === "" && this.password === "";
    }
  },
  mounted() {
    this.$refs.amount.focus();
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
      this.password = "";
      this.$refs.amount.focus();
    },
    onPasswordKeydown() {
      this.isPasswordInvalid = false;
    },
    showConfirmation() {
      // amount is always less then or equal to the floored spendable amount
      // if useMax is checked, use the maxAmount and subtract the fee from the amount
      let amount = this.useMax ? this.maxAmount : displayToMonetary(this.amount);

      // validate address
      this.isAddressInvalid = !LibraryController.IsValidNativeAddress(this.address);

      // validate password (confirmation dialog unlocks/locks when user confirms so don't leave it unlocked here)
      this.isPasswordInvalid = !this.validatePassword(this.computedPassword);

      if (this.hasErrors) return;

      EventBus.$emit("show-dialog", {
        title: this.$t("send_coins.confirm_transaction"),
        component: ConfirmTransactionDialog,
        componentProps: {
          amount: amount,
          address: this.address,
          password: this.password,
          subtractFee: this.useMax
        },
        showButtons: false
      });
    },
    onTransactionSucceeded() {
      this.$router.push({ name: "transactions" });
    },
    validatePassword(password) {
      // validation can only be done by unlocking the wallet, but make sure to lock the wallet afterwards
      return LibraryController.UnlockWallet(password, 0);
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

button.max,
button.clear {
  margin-left: 10px;
  padding: 0 15px;
}

button.send {
  width: 100%;
}
</style>

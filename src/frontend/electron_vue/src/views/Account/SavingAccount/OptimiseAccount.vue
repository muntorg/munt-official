<template>
  <div class="send-coins flex-col">
    <portal to="sidebar-right-title">
      {{ $t("buttons.optimise") }}
    </portal>

    <div class="main">
      <p>
        {{ $t("saving_account.optimise_info", { parts: parts, futureOptimalAmount: futureOptimalAmount }) }}
      </p>
      <app-form-field title="saving_account.optimise_holding_account">
        <select-list :options="fundingAccounts" :default="fundingAccount" v-model="fundingAccount" />
      </app-form-field>

      <input
        v-model="password"
        type="password"
        v-show="walletPassword === null"
        :placeholder="$t('common.enter_your_password')"
        :class="passwordClass"
        @keydown="onPasswordKeydown"
      />
    </div>
    <button @click="tryOptimise" :disabled="disableSendButton">
      {{ $t("buttons.optimise") }}
    </button>
  </div>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import { LibraryController, AccountsController, WitnessController } from "../../../unity/Controllers";
import EventBus from "../../../EventBus";

export default {
  name: "OptimiseAccount",
  data() {
    return {
      password: null,
      fundingAccount: null,
      isAmountInvalid: false,
      isPasswordInvalid: false,
      parts: null,
      futureOptimalAmount: null
    };
  },
  computed: {
    ...mapState("wallet", ["walletPassword"]),
    ...mapGetters("wallet", ["accounts", "account"]),
    computedPassword() {
      return this.walletPassword ? this.walletPassword : this.password || "";
    },
    passwordClass() {
      return this.isPasswordInvalid ? "error" : "";
    },
    fundingAccounts() {
      return this.accounts.filter(x => x.state === "Normal" && ["Desktop"].indexOf(x.type) !== -1);
    },
    hasErrors() {
      return this.isPasswordInvalid;
    },
    disableSendButton() {
      if (this.computedPassword.trim().length === 0) return true;
      return false;
    },
    accountParts() {
      return this.getStatistics("account_parts");
    }
  },
  mounted() {
    const stats = WitnessController.GetAccountWitnessStatistics(this.account.UUID);
    this.parts = stats["account_parts"];

    let optimalAmounts = WitnessController.GetOptimalWitnessDistributionForAccount(this.account.UUID);
    this.futureOptimalAmount = optimalAmounts.length;

    if (this.fundingAccounts.length) {
      this.fundingAccount = this.fundingAccounts[0];
    }
  },
  methods: {
    onPasswordKeydown() {
      this.isPasswordInvalid = false;
    },
    tryOptimise() {
      if (LibraryController.UnlockWallet(this.computedPassword, 120) === false) {
        this.isPasswordInvalid = true;
      }

      if (this.hasErrors) {
        alert("Invalid password");
        this.$store.dispatch("app/SET_ACTIVITY_INDICATOR", false);
        return;
      }

      this.$store.dispatch("app/SET_ACTIVITY_INDICATOR", true);

      let accountUUID = AccountsController.GetActiveAccount();
      let optimalAmounts = WitnessController.GetOptimalWitnessDistributionForAccount(accountUUID);

      let result = WitnessController.OptimiseWitnessAccount(this.account.UUID, this.fundingAccount.UUID, optimalAmounts);

      if (result.result === true) {
        this.password = null;
        this.fundingAccount = null;

        this.$store.dispatch("app/SET_ACTIVITY_INDICATOR", false);
        EventBus.$emit("close-right-sidebar");
        alert("Operation was successful");
      } else {
        alert(result.info);
        this.$store.dispatch("app/SET_ACTIVITY_INDICATOR", false);
      }

      LibraryController.LockWallet();
    }
  }
};
</script>

<style lang="less" scoped>
.send-coins {
  height: 100%;

  .main {
    flex: 1;
  }
}

button {
  width: 100%;
}
</style>

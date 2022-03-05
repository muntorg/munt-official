<template>
  <div class="send-coins flex-col">
    <portal to="sidebar-right-title">
      {{ $t("buttons.renew") }}
    </portal>

    <div class="main">
      <app-form-field :title="$t('renew_holding_account.funding_account')">
        <select-list
          :options="fundingAccounts"
          :default="fundingAccount"
          v-model="fundingAccount"
        />
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
    <button @click="trySend" :disabled="disableSendButton">
      {{ $t("buttons.send") }}
    </button>
  </div>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import {
  LibraryController,
  AccountsController,
  WitnessController
} from "../../../unity/Controllers";
import EventBus from "../../../EventBus";

export default {
  name: "RenewAccount",
  data() {
    return {
      password: null,
      fundingAccount: null,
      isAmountInvalid: false,
      isPasswordInvalid: false
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
      return this.accounts.filter(
        x => x.state === "Normal" && ["Desktop"].indexOf(x.type) !== -1
      );
    },
    hasErrors() {
      return this.isPasswordInvalid;
    },
    disableSendButton() {
      if (this.computedPassword.trim().length === 0) return true;
      return false;
    }
  },
  mounted() {
    if (this.fundingAccounts.length) {
      this.fundingAccount = this.fundingAccounts[0];
    }
  },
  methods: {
    onPasswordKeydown() {
      this.isPasswordInvalid = false;
    },
    trySend() {
      /*
       todo:
        - improve notifications / messages on success and error
       */

      // wallet needs to be unlocked to make a payment
      if (LibraryController.UnlockWallet(this.computedPassword) === false) {
        this.isPasswordInvalid = true;
      }

      if (this.hasErrors) return;

      let accountUUID = AccountsController.GetActiveAccount();
      let result = WitnessController.RenewWitnessAccount(
        this.fundingAccount.UUID,
        accountUUID
      );

      if (result.status === "success") {
        this.password = null;
        this.fundingAccount = null;

        EventBus.$emit("close-right-sidebar");
      } else {
        console.log(result);
      }

      // lock the wallet again
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

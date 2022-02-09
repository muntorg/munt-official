<template>
  <div class="add-spending-account">
    <portal to="header-slot">
      <main-header :title="$t('add_spending_account.title')"></main-header>
    </portal>

    <section class="content">
      <section>
        <app-form-field :title="$t('common.account_name')">
          <input
            type="text"
            v-model="accountName"
            maxlength="30"
            ref="accountName"
          />
        </app-form-field>
        <app-form-field
          :title="$t('common.password')"
          v-if="walletPassword === null"
        >
          <input
            v-model="password"
            type="password"
            :class="passwordClass"
            @keydown="onPasswordKeydown"
          />
        </app-form-field>
      </section>
    </section>
    <portal to="footer-slot">
      <app-button-section>
        <button @click="addSpendingAccount" :disabled="accountName.length < 3">
          {{ $t("buttons.done") }}
        </button>
      </app-button-section>
    </portal>
  </div>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import {
  AccountsController,
  LibraryController
} from "../../../unity/Controllers";

export default {
  name: "AddSpendingAccount",
  data() {
    return {
      accountName: "",
      password: "",
           isPasswordInvalid: false
    };
  },
  computed: {
    ...mapState("wallet", ["walletPassword"]),
    ...mapGetters("wallet", ["accounts"]),
    computedPassword() {
      return this.walletPassword ? this.walletPassword : this.password || "";
    },
    passwordClass() {
      return this.isPasswordInvalid ? "error" : "";
    },
    hasErrors() {
      return this.isPasswordInvalid;
    },
    disableLockButton() {
      if (this.accountName.trim().length === 0) return true;
      if (this.computedPassword.trim().length === 0) return true;
      return false;
    }
  },
  methods: {
    onPasswordKeydown() {
      this.isPasswordInvalid = false;
    },
    addSpendingAccount() {
      if (LibraryController.UnlockWallet(this.computedPassword) === false) {
        this.isPasswordInvalid = true;
      }

      if (this.hasErrors) return;

      let accountId = AccountsController.CreateAccount(this.accountName, "HD");

      if (accountId) {
        this.$router.push({ name: "account", params: { id: accountId } });
      } else {
        console.log("Error");
        AccountsController.DeleteAccount(accountId); // something went wrong, so delete the account
      }

      LibraryController.LockWallet();
    }
  },
  mounted() {
    console.log(this.walletPassword);
  }
};
</script>

<style lang="less" scoped>
.earnings {
  line-height: 1.2em;
}
.lock-time-slider {
  width: calc(100% - 100px) !important;
  display: inline-block;
}
.lock-time-info {
  text-align: right;
  line-height: 18px;
  flex: 1;
}
</style>

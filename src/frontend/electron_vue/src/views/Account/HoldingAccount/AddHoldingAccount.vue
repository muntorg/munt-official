<template>
  <div class="add-holding-account">
    <portal to="header-slot">
      <main-header :title="$t('add_holding_account.title')"></main-header>
    </portal>

    <section class="content">
      <novo-form-field title="account name">
        <input
          type="text"
          v-model="accountName"
          maxlength="30"
          ref="accountName"
          :placeholder="$t('common.account_name')"
        />
      </novo-form-field>
      <novo-form-field title="funding account">
        <select v-model="fundingAccount">
          <option
            v-for="account in fundingAccounts"
            :key="account.UUID"
            :value="account"
            >{{ account.label }}</option
          >
        </select>
      </novo-form-field>
      <novo-form-field title="amount">
        <input
          type="number"
          min="50"
          v-model="amount"
          :max="maxAmounForAccount"
        />
      </novo-form-field>
      <novo-form-field title="lock time (months)">
        <vue-slider
          :min="2"
          :max="36"
          :value="lockTimeInMonths"
          v-model="lockTimeInMonths"
        />
      </novo-form-field>

      <novo-form-field title="password">
        <input
          v-model="password"
          type="password"
          v-show="walletPassword === null"
          :placeholder="$t('common.enter_password')"
          :class="passwordClass"
          @keydown="onPasswordKeydown"
        />
      </novo-form-field>

      <pre>{{ estimatedWeight }}</pre>
    </section>

    <portal to="footer-slot">
      <novo-button-section>
        <button @click="createAndFundHoldingAccount" :disabled="disableButton">
          {{ $t("buttons.fund") }}
        </button>
      </novo-button-section>
    </portal>
  </div>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import UnityBackend from "../../../unity/UnityBackend";

export default {
  name: "AddHoldingAccount",
  data() {
    return {
      accountName: "",
      fundingAccount: null,
      amount: 50,
      lockTimeInMonths: 2,
      password: "",
      isPasswordInvalid: false
    };
  },
  computed: {
    ...mapState(["walletPassword"]),
    ...mapGetters(["accounts"]),
    computedPassword() {
      return this.walletPassword ? this.walletPassword : this.password || "";
    },
    fundingAccounts() {
      return this.accounts.filter(
        x =>
          x.state === "Normal" &&
          ["Desktop", "Mining"].indexOf(x.type) !== -1 &&
          x.balance >= 50
      );
    },
    maxAmounForAccount() {
      return this.fundingAccount ? this.fundingAccount.balance : 0;
    },
    estimatedWeight() {
      let estimation = UnityBackend.GetEstimatedWeight(
        this.amount * 100000000,
        Math.floor((1095 / 36) * this.lockTimeInMonths)
      );
      return estimation;
    },
    passwordClass() {
      return this.isPasswordInvalid ? "error" : "";
    },
    hasErrors() {
      return this.isPasswordInvalid;
    },
    disableButton() {
      if (this.accountName.trim().length === 0) return true;
      if (this.computedPassword.trim().length === 0) return true;
      return false;
    }
  },
  mounted() {
    this.$refs.accountName.focus();
    if (this.fundingAccounts.length) {
      this.fundingAccount = this.fundingAccounts[0];
    }
  },
  methods: {
    onPasswordKeydown() {
      this.isPasswordInvalid = false;
    },
    createAndFundHoldingAccount() {
      // wallet needs to be unlocked to make a payment
      if (UnityBackend.UnlockWallet(this.computedPassword) === false) {
        this.isPasswordInvalid = true;
      }

      if (this.hasErrors) return;

      UnityBackend.UnlockWallet(this.computedPassword);
      let accountId = UnityBackend.CreateAccount(this.accountName, "Witness");

      console.log(this.fundingAccount);
      console.log(accountId);

      let result = UnityBackend.FundWitnessAccount(
        this.fundingAccount.UUID,
        accountId,
        this.amount * 100000000,
        this.lockTimeInMonths * 43800
      );

      console.log(result);

      UnityBackend.LockWallet();
    }
  }
};
</script>

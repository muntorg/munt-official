<template>
  <div class="add-holding-account">
    <portal to="header-slot">
      <main-header :title="$t('add_holding_account.title')"></main-header>
    </portal>

    <section class="content">
      <section class="step-1" v-if="current === 1">
        <app-form-field :title="$t('add_holding_account.funding_account')">
          <select-list
            :options="fundingAccounts"
            :default="fundingAccount"
            v-model="fundingAccount"
          />
        </app-form-field>
        <app-form-field :title="$t('common.amount')">
          <input
            type="number"
            min="50"
            v-model="amount"
            :max="maxAmountForAccount"
            :class="amountClass"
          />
        </app-form-field>
        <app-form-field :title="$t('add_holding_account.lock_for')">
          <div class="flex-row">
            <vue-slider
              :min="2"
              :max="36"
              class="lock-time-slider"
              :class="lockTimeClass"
              :value="lockTimeInMonths"
              v-model="lockTimeInMonths"
            />
            <div class="lock-time-info">
              {{ lockTimeInMonths }} {{ $t("common.months") }}
            </div>
          </div>
        </app-form-field>

        <app-form-field
          :title="$t('add_holding_account.estimated_earnings')"
          v-if="isWeightSufficient"
        >
          <div class="flex-row">
            <div class="earnings">{{ $t("add_holding_account.daily") }}</div>
            <div class="flex-1 align-right">
              {{
                this.formatMoneyForDisplay(
                  this.estimatedWeight.estimated_daily_earnings
                )
              }}
            </div>
          </div>
          <div class="flex-row">
            <div class="earnings">{{ $t("add_holding_account.overall") }}</div>
            <div class="flex-1 align-right">
              {{
                this.formatMoneyForDisplay(
                  this.estimatedWeight.estimated_lifetime_earnings
                )
              }}
            </div>
          </div>
        </app-form-field>
      </section>
      <section class="step-2" v-else>
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
        <template v-slot:left>
          <button @click="current--" v-if="current !== 1">
            {{ $t("buttons.previous") }}
          </button>
        </template>
        <button
          @click="nextStep"
          :disabled="!isWeightSufficient"
          v-if="current === 1"
        >
          {{ $t("buttons.next") }}
        </button>
        <button
          @click="createAndFundHoldingAccount"
          :disabled="disableLockButton"
          v-else
        >
          {{ $t("buttons.lock") }}
        </button>
      </app-button-section>
    </portal>
  </div>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import { formatMoneyForDisplay, displayToMonetary } from "../../../util.js";
import {
  WitnessController,
  LibraryController,
  AccountsController
} from "../../../unity/Controllers";

export default {
  name: "AddHoldingAccount",
  data() {
    return {
      current: 1,
      networkLimits: null,
      accountName: "",
      fundingAccount: null,
      amount: 50,
      lockTimeInMonths: 36,
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
    amountClass() {
      return this.amount <
        parseInt(this.networkLimits.minimum_witness_amount) ||
        this.amount > this.maxAmountForAccount
        ? "error"
        : "";
    },
    fundingAccounts() {
      return this.accounts.filter(
        x =>
          x.state === "Normal" &&
          ["Desktop", "Mining"].indexOf(x.type) !== -1 &&
          x.balance >= 50
      );
    },
    maxAmountForAccount() {
      return this.fundingAccount
        ? Math.floor(this.fundingAccount.balance / 100000000) // make sure balance is divided by 100000000
        : 0;
    },
    lockTimeInBlocks() {
      return (
        this.lockTimeInMonths *
        (this.networkLimits.maximum_lock_period_blocks / 36)
      );
    },
    isWeightSufficient() {
      return (
        this.estimatedWeight.weight >= this.networkLimits.minimum_witness_weight
      );
    },
    lockTimeClass() {
      return this.isWeightSufficient ? "" : "insufficient";
    },
    estimatedWeight() {
      let estimation = WitnessController.GetEstimatedWeight(
        displayToMonetary(this.amount),
        this.lockTimeInBlocks
      );

      return estimation;
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
  created() {
    this.networkLimits = WitnessController.GetNetworkLimits();
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
    formatMoneyForDisplay(amount) {
      return formatMoneyForDisplay(amount);
    },
    nextStep() {
      this.current++;
      this.$nextTick(() => {
        this.$refs.accountName.focus();
      });
    },
    createAndFundHoldingAccount() {
      let result = null;
      let uuid = null;

      try {
        this.$store.dispatch("app/SET_ACTIVITY_INDICATOR", true);

        // wallet needs to be unlocked to make a payment
        if (LibraryController.UnlockWallet(this.computedPassword) === false) {
          this.isPasswordInvalid = true;
        }

        if (this.hasErrors) return;

        uuid = AccountsController.CreateAccount(this.accountName, "Holding");

        // Always lock for slightly longer than the minimum to allow a bit of time for transaction to enter the chain
        // If we don't then its possible that the transaction becomes invalid before entering the chain
        let finalLockTime =
          this.lockTimeInBlocks + 50 <
          this.networkLimits.maximum_lock_period_blocks
            ? this.lockTimeInBlocks + 50
            : this.lockTimeInBlocks;

        result = WitnessController.FundWitnessAccount(
          this.fundingAccount.UUID,
          uuid,
          this.amount * 100000000,
          finalLockTime
        );

        if (result.status !== "success") {
          AccountsController.DeleteAccount(uuid); // something went wrong, so delete the account
        }

        LibraryController.LockWallet();
      } finally {
        if (result.status === "success") {
          // route to the holding account when successfully created and funded
          this.$router.push({ name: "account", params: { id: uuid } });
        } else {
          // remove the activity indicator
          this.$store.dispatch("app/SET_ACTIVITY_INDICATOR", false);
        }
      }
    }
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
input {
  background-color: #eee;
}
</style>

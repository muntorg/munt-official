<template>
  <div class="setup-mining">
    <portal to="header-slot">
      <main-header :title="$t('setup_mining.title')"></main-header>
    </portal>

    <app-section>
      <div class="mining-info">
        {{ $t("setup_mining.information") }}
      </div>
    </app-section>
    <app-form-field :title="$t('common.password')">
      <input
        type="password"
        v-model="password"
        :class="computedStatus"
        @keydown="createMiningAccountOnEnter"
      />
    </app-form-field>

    <portal to="footer-slot">
      <app-button-section>
        <button
          @click="createMiningAccount(password)"
          :disabled="!isEnableMiningButtonEnabled"
        >
          {{ $t("buttons.create_mining_account") }}
        </button>
      </app-button-section>
    </portal>
  </div>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import {
  LibraryController,
  AccountsController
} from "../../../unity/Controllers";

export default {
  name: "SetupMining",
  data() {
    return {
      password: null,
      isPasswordInvalid: false
    };
  },
  computed: {
    ...mapState("wallet", ["walletPassword"]),
    ...mapGetters("wallet", ["miningAccount"]),
    computedStatus() {
      return this.isPasswordInvalid ? "error" : "";
    },
    isEnableMiningButtonEnabled() {
      return this.password && this.password.trim().length > 0;
    }
  },
  watch: {
    walletPassword() {
      this.createMiningAccount(this.walletPassword);
    }
  },
  created() {
    if (this.walletPassword) {
      this.createMiningAccount(this.walletPassword);
    }
  },
  methods: {
    createMiningAccountOnEnter() {
      this.isPasswordInvalid = false;
      if (event.keyCode === 13) this.createMiningAccount(this.password);
    },
    createMiningAccount(password) {
      let uuid = null;

      try {
        // NOTE:
        // Dont' know if it is actually needed to show the activity indicator when unlockking the wallet and creating the account,
        // but for now I leave it here.
        this.$store.dispatch("app/SET_ACTIVITY_INDICATOR", true);
        if (LibraryController.UnlockWallet(password) === false) {
          this.isPasswordInvalid = true;
          return;
        }

        uuid = AccountsController.CreateAccount("Florin Mining", "Mining");
        LibraryController.LockWallet();
      } finally {
        // route to the new account when we have a uuid
        if (uuid) {
          // activity indicator is set to true in the router, so no need to remove it here
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
.mining-info {
  line-height: 1.2em;
}
</style>

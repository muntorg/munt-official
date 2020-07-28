<template>
  <account-page-layout class="setup-mining">
    <template v-slot:header>
      <section class="header flex-row">
        <div class="info">
          <div class="label">
            {{ $t("setup_mining.title") }}
          </div>
        </div>
      </section>
    </template>

    <novo-section>
      {{ $t("setup_mining.information") }}
    </novo-section>
    <novo-form-field :title="$t('common.password')">
      <input
        type="password"
        v-model="password"
        :class="computedStatus"
        @keydown="createMiningAccountOnEnter"
      />
    </novo-form-field>

    <template v-slot:footer>
      <section class="footer">
        <button
          @click="createMiningAccount(password)"
          :disabled="!isEnableMiningButtonEnabled"
        >
          {{ $t("buttons.create_mining_account") }}
        </button>
      </section>
    </template>
  </account-page-layout>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import UnityBackend from "../../../unity/UnityBackend";
import AccountPageLayout from "../AccountPageLayout";

export default {
  name: "SetupMining",
  data() {
    return {
      password: null,
      isPasswordInvalid: false
    };
  },
  components: {
    AccountPageLayout
  },
  computed: {
    ...mapState(["walletPassword"]),
    ...mapGetters(["miningAccount"]),
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
      if (UnityBackend.UnlockWallet(password) === false) {
        this.isPasswordInvalid = true;
        return;
      }
      let uuid = UnityBackend.CreateAccount("Novo Mining", "Mining");
      UnityBackend.LockWallet();
      this.$router.push({ name: "account", params: { id: uuid } });
    }
  }
};
</script>

<style lang="less" scoped>
::v-deep {
  & .header {
    & > .info {
      width: calc(100% - 48px - 26px);
      padding-right: 10px;

      & > .label {
        font-size: 1.1em;
        font-weight: 500;
        line-height: 20px;
      }
    }
  }

  & .footer {
    text-align: right;
    padding-right: 5px;
  }
}
</style>

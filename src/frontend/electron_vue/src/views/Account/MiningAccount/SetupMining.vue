<template>
  <div class="setup-mining">
    <portal to="header-slot">
      <main-header :title="$t('setup_mining.title')"></main-header>
    </portal>

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

    <portal to="footer-slot">
      <novo-button-section>
        <button
          @click="createMiningAccount(password)"
          :disabled="!isEnableMiningButtonEnabled"
        >
          {{ $t("buttons.create_mining_account") }}
        </button>
      </novo-button-section>
    </portal>
  </div>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import UnityBackend from "../../../unity/UnityBackend";

export default {
  name: "SetupMining",
  data() {
    return {
      password: null,
      isPasswordInvalid: false
    };
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
.title {
  font-size: 1.1em;
  font-weight: 500;
  line-height: var(--header-height);
}
</style>

<template>
  <div class="setup-mining">
    <div class="main-section">
      <div class="header">
        <div class="info">
          <div class="label">
            {{ $t("setup_mining.title") }}
          </div>
        </div>
      </div>
      <div class="content">
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
      </div>
      <div class="footer">
        <novo-button-section>
          <button
            @click="createMiningAccount(password)"
            :disabled="!isEnableMiningButtonEnabled"
          >
            {{ $t("buttons.create_mining_account") }}
          </button>
        </novo-button-section>
      </div>
    </div>
  </div>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import UnityBackend from "../unity/UnityBackend";

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
.setup-mining {
  height: 100vh;
}

.main-section {
  height: 100%;
  flex: 1;

  display: flex;
  flex-direction: column;

  & .header {
    height: var(--header-height);
    border-bottom: 1px solid var(--main-border-color);
    padding: 11px 24px 0 24px;

    & .info {
      & .label {
        font-size: 1.1em;
        font-weight: 500;
        line-height: 20px;
      }
    }
  }

  & .content {
    flex: 1;
    padding: 24px;
    overflow-y: hidden;
  }

  & .footer {
    height: var(--footer-height);
    line-height: var(--footer-height);
    border-top: 1px solid var(--main-border-color);
    text-align: center;
    padding-right: 10px;
  }
}
</style>

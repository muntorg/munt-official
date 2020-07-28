<template>
  <section class="wallet-layout flex-row">
    <section class="sidebar flex-col">
      <header class="flex-row">
        <div class="logo" />
        <div class="total-balance">
          {{ totalBalance }}
        </div>
      </header>
      <accounts-section class="accounts" />
      <footer class="flex-row">
        <div class="status" />
        <div class="button" @click="changeLockSettings">
          <fa-icon :icon="['fal', lockIcon]" />
        </div>
        <div class="button" @click="showMining">
          <fa-icon :icon="['fal', 'gem']" />
        </div>
        <div class="button" @click="showSettings">
          <fa-icon :icon="['fal', 'user-circle']" />
        </div>
      </footer>
    </section>
    <router-view class="main" />
  </section>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import AccountsSection from "./AccountsSection";
import WalletPasswordDialog from "../components/WalletPasswordDialog";
import EventBus from "../EventBus";

export default {
  name: "WalletLayout",
  data() {
    return {};
  },
  components: {
    AccountsSection
  },
  computed: {
    ...mapState(["activeAccount", "walletPassword"]),
    ...mapGetters(["totalBalance", "accounts", "miningAccount"]),
    lockIcon() {
      return this.walletPassword ? "unlock" : "lock";
    }
  },
  methods: {
    showMining() {
      if (this.miningAccount) {
        if (this.$route.path === `/account/${this.miningAccount.UUID}`) return;
        this.$router.push({
          name: "account",
          params: { id: this.miningAccount.UUID }
        });
      } else {
        if (this.$route.name === "setup-mining") return;
        this.$router.push({ name: "setup-mining" });
      }
    },
    showSettings() {
      if (this.$route.path === "/settings/") return;
      this.$router.push({ name: "settings" });
    },
    changeLockSettings() {
      if (this.walletPassword) {
        this.$store.dispatch({
          type: "SET_WALLET_PASSWORD",
          walletPassword: null
        });
      } else {
        EventBus.$emit("show-dialog", {
          title: this.$t("password_dialog.unlock_wallet"),
          component: WalletPasswordDialog,
          showButtons: false
        });
      }
    }
  }
};
</script>

<style lang="less" scoped>
.wallet-layout {
  width: 100%;
  height: 100vh;
  overflow: hidden;

  & > .sidebar {
    flex: 0 0 var(--sidebar-width);
    max-width: var(--sidebar-width);
    background: var(--sidebar-background-color);
    color: var(--sidebar-color);

    & > header {
      height: var(--header-height);
      border-bottom: 1px solid var(--sidebar-border-color);
    }

    & > .accounts {
      height: calc(100% - var(--header-height) - var(--footer-height));
    }

    & > footer {
      height: var(--footer-height);
      border-top: 1px solid var(--sidebar-border-color);
    }
  }

  & > .main {
    flex: 0 0 calc(100% - var(--sidebar-width));
  }
}

.sidebar > header {
  padding: 20px;
  color: #fff;

  & .logo {
    width: 22px;
    min-width: 22px;
    height: 22px;
    min-height: 22px;
    background: url("../img/logo.svg"),
      linear-gradient(transparent, transparent);
    background-size: cover;
  }

  & .total-balance {
    padding: 0 0 0 10px;
    line-height: 22px;
    overflow: hidden;
    text-overflow: ellipsis;
  }
}

.sidebar > footer {
  font-size: 16px;
  font-weight: 400;

  & .status {
    flex: 1;
  }

  & .button {
    line-height: 52px;
    padding: 0 20px;
    cursor: pointer;

    &:hover {
      background-color: #222;
    }
  }
}
</style>

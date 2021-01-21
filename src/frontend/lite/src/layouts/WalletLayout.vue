<template>
  <section class="wallet-layout">
    <section class="header flex-row">
      <div class="logo" />
      <div class="balance flex-row">
        <div class="coin">{{ totalBalance.toFixed(2) }}</div>
        <div class="fiat">€ {{ totalBalance.toFixed(2) }}</div>
      </div>
      <div class="flex-1" />
      <div class="button" @click="showSettings">
        <fa-icon :icon="['fal', 'cog']" />
      </div>
      <div class="button" @click="changeLockSettings">
        <fa-icon :icon="['fal', lockIcon]" />
      </div>
    </section>
    <section class="content scrollable">
      <router-view />
    </section>
    <section class="footer flex-row">
      <div class="flex-1" />
      <div
        :class="getButtonClassNames('transactions')"
        @click="routeTo('transactions')"
      >
        <fa-icon :icon="['far', 'list-ul']" />
        {{ $t("buttons.transactions") }}
      </div>
      <div :class="getButtonClassNames('send')" @click="routeTo('send')">
        <fa-icon :icon="['fas', 'arrow-circle-up']" />
        {{ $t("buttons.send") }}
      </div>
      <div :class="getButtonClassNames('receive')" @click="routeTo('receive')">
        <fa-icon :icon="['fas', 'arrow-circle-down']" />
        {{ $t("buttons.receive") }}
      </div>
      <div class="flex-1" />
    </section>
  </section>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import WalletPasswordDialog from "../components/WalletPasswordDialog";
import EventBus from "../EventBus";

export default {
  name: "WalletLayout",
  computed: {
    ...mapState("app", ["progress"]),
    ...mapState("wallet", ["activeAccount", "walletPassword"]),
    ...mapGetters("wallet", ["totalBalance"]),
    lockIcon() {
      return this.walletPassword ? "unlock" : "lock";
    }
  },
  watch: {
    progress() {
      console.log(`${this.progress}`);
    }
  },
  methods: {
    routeTo(route) {
      this.$router.push({ name: route });
    },
    getButtonClassNames(route) {
      let classNames = ["button"];
      if (route === this.$route.name) classNames.push("active");
      return classNames;
    },
    showSettings() {
      if (this.$route.path === "/settings/") return;
      this.$router.push({ name: "settings" });
    },
    changeLockSettings() {
      if (this.walletPassword) {
        this.$store.dispatch("wallet/SET_WALLET_PASSWORD", null);
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
  height: 100vh;
  overflow: hidden;

  & > .header {
    height: var(--header-height);
    padding: 10px 20px 10px 20px;
    border-bottom: 1px solid var(--main-border-color);
    font-size: 1.1em;
    line-height: 42px;

    & > .logo {
      margin: 10px;
      width: 22px;
      min-width: 22px;
      height: 22px;
      min-height: 22px;
      background: url("../img/logo-start.svg"),
        linear-gradient(transparent, transparent);
      background-size: cover;
    }
    & > .balance {
      & > .coin {
        font-weight: 600;
      }
      & > .fiat {
        margin: 0 0 0 10px;
      }
    }
    & .button {
      line-height: 42px;
      font-size: 1.2em;
      font-weight: 300;
      padding: 0 10px;
      cursor: pointer;

      &:hover {
        color: var(--primary-color);
        background-color: #eff3ff;
      }
    }
  }

  & > .content {
    height: calc(100% - var(--header-height) - var(--footer-height));
    padding: 20px;
  }

  & > .footer {
    height: var(--footer-height);
    border-top: 1px solid var(--main-border-color);
    line-height: 32px;
    padding: 10px 20px;

    font-size: 1em;
    font-weight: 500;

    & > .button {
      padding: 0 30px;
      cursor: pointer;
    }

    & > .button.active {
      color: var(--primary-color);
    }

    & > .button:not(.active):hover {
      color: var(--primary-color);
      background-color: #eff3ff;
    }
  }
}
</style>

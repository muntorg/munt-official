<template>
  <section class="wallet-layout flex-row" :class="walletLayoutClasses">
    <section v-if="!isSingleAccount" class="sidebar-left">
      <section class="header flex-row">
        <div class="logo" />
        <div class="total-balance flex-row">
          <account-tooltip type="Wallet" :account="account">
            <div class="flex-row">
              <div class="coin">
                {{ balanceForDisplay }}
              </div>
              <div class="fiat">{{ totalBalanceFiat }}</div>
            </div>
          </account-tooltip>
        </div>
      </section>
      <accounts-section class="accounts" />
      <section class="footer flex-row">
        <div class="status" />
        <div class="button" @click="handleWalletLock">
          <fa-icon :icon="['fal', lockIcon]" />
        </div>
        <div v-if="!isSPV" class="button" @click="showMining">
          <fa-icon :icon="['fal', 'gem']" />
        </div>
        <div class="button" @click="showSettings">
          <fa-icon :icon="['fal', 'user-circle']" />
        </div>
      </section>
    </section>
    <section class="main">
      <section class="header">
        <account-header v-if="isSingleAccount" :account="account" :is-single-account="true"></account-header>
        <portal-target v-else ref="headerSlot" name="header-slot" @change="headerSlotChanged"></portal-target>
      </section>
      <section class="content scrollable">
        <router-view />
      </section>
      <section class="footer">
        <div v-if="isSingleAccount" style="display: flex">
          <footer-button title="buttons.transactions" :icon="['far', 'list-ul']" routeName="account" @click="routeTo" />
          <footer-button title="buttons.send" :icon="['fal', 'arrow-from-bottom']" routeName="send" @click="routeTo" />
          <footer-button title="buttons.receive" :icon="['fal', 'arrow-to-bottom']" routeName="receive" @click="routeTo" />
        </div>
        <portal-target v-else ref="footerSlot" name="footer-slot" @change="footerSlotChanged"></portal-target>
      </section>
    </section>
  </section>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import { formatMoneyForDisplay } from "../util.js";
import EventBus from "../EventBus";
import UIConfig from "../../ui-config.json";
import { AccountsController } from "../unity/Controllers";
import AccountsSection from "./AccountsSection.vue";
import AccountTooltip from "../components/AccountTooltip.vue";
import AccountHeader from "../components/AccountHeader.vue";

export default {
  name: "WalletLayout",
  data() {
    return {
      isHeaderSlotEmpty: true,
      isFooterSlotEmpty: true,
      isSingleAccount: UIConfig.isSingleAccount,
      isSPV: UIConfig.isSPV
    };
  },
  components: {
    AccountsSection,
    AccountHeader,
    AccountTooltip
  },
  computed: {
    ...mapState("app", ["progress", "rate"]),
    ...mapState("wallet", ["activeAccount", "unlocked"]),
    ...mapGetters("wallet", ["totalBalance", "miningAccount", "account"]),
    walletLayoutClasses() {
      let classes = [];
      if (!this.isSingleAccount) {
        if (this.isHeaderSlotEmpty) classes.push("no-header");
        if (this.isFooterSlotEmpty) classes.push("no-footer");
      } else {
        classes.push("no-sidebar-left");
      }
      return classes;
    },
    lockIcon() {
      return this.unlocked ? "unlock" : "lock";
    },
    totalBalanceFiat() {
      if (!this.rate) return "";
      return `€ ${formatMoneyForDisplay(this.totalBalance * this.rate, true)}`;
    },
    balanceForDisplay() {
      if (this.totalBalance == null) return "";
      return formatMoneyForDisplay(this.totalBalance);
    }
  },
  methods: {
    headerSlotChanged(newContent) {
      this.isHeaderSlotEmpty = !newContent;
    },
    footerSlotChanged(newContent) {
      this.isFooterSlotEmpty = !newContent;
    },
    showMining() {
      if (this.miningAccount) {
        if (this.$route.path.indexOf("/account") == 0 && this.miningAccount.UUID === this.activeAccount) return;
        this.$router.push({
          name: "account",
          params: { id: this.miningAccount.UUID }
        });
      } else {
        EventBus.$emit("unlock-wallet", {
          timeout: 5,
          title: "setup_mining.title",
          message: "setup_mining.information",
          callback: async () => {
            let uuid = null;
            try {
              // NOTE:
              // Dont' know if it is actually needed to show the activity indicator when unlocking the wallet and creating the account, but for now I leave it here.
              this.$store.dispatch("app/SET_ACTIVITY_INDICATOR", true);
              uuid = AccountsController.CreateAccount("Mining", "Mining");
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
        });
      }
    },
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
    handleWalletLock() {
      EventBus.$emit(this.unlocked ? "lock-wallet" : "unlock-wallet");
    }
  }
};
</script>

<style lang="less" scoped>
.wallet-layout {
  height: 100vh;
  overflow: hidden;

  --header-height-main: var(--header-height);
  --footer-height-main: var(--footer-height);

  &.no-header {
    --header-height-main: 0px;

    & > .main > .header {
      display: none;
    }
  }

  &.no-footer {
    --footer-height-main: 0px;
    & > .main > .footer {
      display: none;
    }
  }

  & > .sidebar-left {
    width: var(--sidebar-left-width);
    background: var(--sidebar-left-background-color);
    color: var(--sidebar-left-color);

    & > .header {
      height: var(--header-height);
      border-bottom: 1px solid var(--sidebar-left-border-color);
    }

    & > .accounts {
      height: calc(100% - var(--header-height) - var(--footer-height));
    }

    & > .footer {
      height: var(--footer-height);
      border-top: 1px solid var(--sidebar-left-border-color);
    }
  }

  & > .main {
    width: calc(100% - var(--sidebar-left-width));

    & > .header {
      height: var(--header-height);
      border-bottom: 1px solid var(--main-border-color);
      padding: 0 30px;

      & > .logo {
        position: relative;
        top: 50%;
        transform: translateY(-50%);
      }
    }

    & > .content {
      height: calc(100% - var(--header-height-main) - var(--footer-height-main));
      padding: 40px 30px 30px 30px;

      & > * {
        height: 100%;
      }
    }

    & > .footer {
      height: var(--footer-height);
      border-top: 1px solid var(--main-border-color);
      line-height: calc(var(--footer-height) - 2px);
      text-align: center;
    }
  }

  &.no-sidebar-left {
    & > .main {
      width: 100%;
    }
  }
}

.sidebar-left > .header {
  padding: 20px;
  color: #fff;

  & .total-balance {
    padding: 0 0 0 10px;
    line-height: 22px;
  }

  & .fiat {
    margin-left: 10px;
    color: #999;
  }
}

.sidebar-left > .footer {
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

.logo {
  width: 22px;
  min-width: 22px;
  height: 22px;
  min-height: 22px;
  background: url("../img/logo.svg"), linear-gradient(transparent, transparent);
  background-size: cover;
}

@media (max-width: 1000px) {
  .wallet-layout {
    & > .sidebar-left {
      width: var(--sidebar-left-width-small);
    }

    & > .main {
      width: calc(100% - var(--sidebar-left-width-small));
    }
  }
}
</style>

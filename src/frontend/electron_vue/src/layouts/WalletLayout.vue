<template>
  <section class="wallet-layout flex-row" :class="walletLayoutClasses">
    <section v-if="UIConfig.showSidebar" class="sidebar-left">
      <section class="header flex-row">
        <div class="logo" />
        <div class="total-balance flex-row">
          <div class="coin">
            {{ balanceForDisplay }}
          </div>
          <div class="fiat">{{ totalBalanceFiat }}</div>
        </div>
      </section>
      <accounts-section v-if="UIConfig.showSidebar" class="accounts" />
      <section class="footer flex-row">
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
      </section>
    </section>
    <section class="main">
      <portal-target
        ref="headerSlot"
        name="header-slot"
        class="header"
        @change="headerSlotChanged"
      ></portal-target>
      <section class="content scrollable">
        <router-view />
      </section>
      <portal-target
        ref="footerSlot"
        name="footer-slot"
        class="footer"
        @change="footerSlotChanged"
      ></portal-target>
    </section>
    <section class="sidebar-right">
      <section class="header flex-row">
        <div class="title">
          <portal-target name="sidebar-right-title" />
        </div>
        <div class="close" @click="closeRightSidebar">
          <fa-icon :icon="['fal', 'times']" />
        </div>
      </section>
      <portal-target
        class="component"
        ref="sidebarRight"
        name="sidebar-right"
        @change="sidebarRightSlotChanged"
      />
    </section>
  </section>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import { formatMoneyForDisplay } from "../util.js";
import AccountsSection from "./AccountsSection";
import WalletPasswordDialog from "../components/WalletPasswordDialog";
import EventBus from "../EventBus";
import UIConfig from "../../ui-config.json";

export default {
  name: "WalletLayout",
  data() {
    return {
      isHeaderSlotEmpty: true,
      isFooterSlotEmpty: true,
      isSidebarRightSlotEmpty: true,
      UIConfig: UIConfig
    };
  },
  components: {
    AccountsSection
  },
  computed: {
    ...mapState("app", ["progress", "rate"]),
    ...mapState("wallet", ["activeAccount", "walletPassword"]),
    ...mapGetters("wallet", ["totalBalance", "miningAccount"]),
    walletLayoutClasses() {
      let classes = [];
      if (this.isHeaderSlotEmpty) classes.push("no-header");
      if (this.isFooterSlotEmpty) classes.push("no-footer");
      if (this.isSidebarRightSlotEmpty) classes.push("no-sidebar-right");
      if (this.isHideSidebarLeft) classes.push("no-sidebar-left");
      return classes;
    },
    lockIcon() {
      return this.walletPassword ? "unlock" : "lock";
    },
    totalBalanceFiat() {
      if (!this.rate) return "";
      return `€ ${formatMoneyForDisplay(this.totalBalance * this.rate)}`;
    },
    balanceForDisplay() {
      if (this.totalBalance == null) return "";
      return formatMoneyForDisplay(this.totalBalance);
    }
  },
  watch: {
    progress() {
      console.log(`${this.progress}`);
    }
  },
  methods: {
    headerSlotChanged(newContent) {
      this.isHeaderSlotEmpty = !newContent;
    },
    footerSlotChanged(newContent) {
      this.isFooterSlotEmpty = !newContent;
    },
    sidebarRightSlotChanged(newContent) {
      this.isSidebarRightSlotEmpty = !newContent;
    },
    showMining() {
      if (this.miningAccount) {
        if (
          this.$route.path.indexOf("/account") == 0 &&
          this.miningAccount.UUID === this.activeAccount
        )
          return;
        this.$router.push({
          name: "account",
          params: { id: this.miningAccount.UUID }
        });
      } else {
        if (this.$route.name === "setup-mining") return;
        this.$router.push({ name: "setup-mining" });
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
    },
    closeRightSidebar() {
      EventBus.$emit("close-right-sidebar");
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

  &.no-sidebar-right {
    --sidebar-right-width: 0px;

    & > .sidebar-right {
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
    width: calc(100% - var(--sidebar-left-width) - var(--sidebar-right-width));

    & > .header {
      height: var(--header-height);
      border-bottom: 1px solid var(--main-border-color);
      padding: 0 20px;
    }

    & > .content {
      height: calc(
        100% - var(--header-height-main) - var(--footer-height-main)
      );
      padding: 20px;
    }

    & > .footer {
      height: var(--footer-height);
      border-top: 1px solid var(--main-border-color);
      line-height: calc(var(--footer-height) - 2px);
      padding: 0 20px;
      text-align: center;
    }
  }

  & > .sidebar-right {
    width: var(--sidebar-right-width);
    background: var(--sidebar-right-background-color);
  }
}

.sidebar-left > .header {
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
  }

  & .fiat {
    margin-left: 10px;
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

.sidebar-right {
  padding: 0 24px;

  & > .header {
    line-height: 62px;
    font-size: 1.1em;
    font-weight: 500;

    & .title {
      flex: 1;
    }

    & .close {
      cursor: pointer;
    }
  }

  & .component {
    height: calc(100% - 72px);
  }
}

@media (max-width: 900px) {
  .wallet-layout:not(.no-sidebar-right) {
    & > .main {
      display: none;
    }

    & > .sidebar-right {
      width: calc(100% - var(--sidebar-left-width));
    }
  }
}
</style>

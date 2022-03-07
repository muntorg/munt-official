<template>
  <div class="spending-account">
    <portal to="header-slot">
      <account-header :account="account"></account-header>
    </portal>

    <transactions v-if="UIConfig.showSidebar && isAccountView" :mutations="mutations" @tx-hash="onTxHash" :tx-hash="txHash" />

    <router-view />

    <portal to="footer-slot">
      <footer-button :icon="['far', 'list-ul']" routeName="account" @click="routeTo">
        {{ $t("buttons.transactions") }}
      </footer-button>
      <footer-button :icon="['fal', 'arrow-from-bottom']" routeName="send" @click="routeTo">
        {{ $t("buttons.send") }}
      </footer-button>
      <footer-button :icon="['fal', 'arrow-to-bottom']" routeName="receive" @click="routeTo">
        {{ $t("buttons.receive") }}
      </footer-button>
    </portal>

    <portal to="sidebar-right">
      <component v-if="rightSidebar" :is="rightSidebar" v-bind="rightSidebarProps" />
    </portal>
  </div>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import { formatMoneyForDisplay } from "../../../util.js";
import EventBus from "../../../EventBus";
import WalletPasswordDialog from "../../../components/WalletPasswordDialog";

import Transactions from "./Transactions";
import Send from "./Send";
import Receive from "./Receive";
import TransactionDetails from "./TransactionDetails";
import AccountSettings from "../AccountSettings";
import UIConfig from "../../../../ui-config.json";

export default {
  name: "SpendingAccount",
  props: {
    account: null
  },
  data() {
    return {
      rightSidebar: null,
      txHash: null,
      UIConfig: UIConfig,
      isA: true
    };
  },
  mounted() {
    EventBus.$on("close-right-sidebar", this.closeRightSidebar);
  },
  beforeDestroy() {
    EventBus.$off("close-right-sidebar", this.closeRightSidebar);
  },
  components: {
    Transactions
  },
  computed: {
    ...mapState("app", ["rate", "activityIndicator"]),
    ...mapState("wallet", ["mutations", "walletPassword"]),
    ...mapGetters("wallet", ["totalBalance"]),
    lockIcon() {
      return this.walletPassword ? "unlock" : "lock";
    },
    isAccountView() {
      return this.$route.name === "account";
    },
    showSendButton() {
      return !this.rightSidebar || this.rightSidebar !== Send;
    },
    showReceiveButton() {
      return !this.rightSidebar || this.rightSidebar !== Receive;
    },
    rightSidebarProps() {
      if (this.rightSidebar === TransactionDetails) {
        return { txHash: this.txHash };
      }
      if (this.rightSidebar === AccountSettings) {
        return { account: this.account };
      }
      return null;
    },
    totalBalanceFiat() {
      if (!this.rate) return "";
      return `€ ${formatMoneyForDisplay(
        this.account.balance * this.rate,
        true
      )}`;
    },
    balanceForDisplay() {
      if (this.account.balance == null) return "";
      return formatMoneyForDisplay(this.account.balance);
    }
  },
  methods: {
    toggleButton() {
      this.isA = !this.isA;
    },
    routeTo(route) {
      if (this.$route.name === route) return;
      this.$router.push({ name: route, params: { id: this.account.UUID } });
    },
    setRightSidebar(name) {
      switch (name) {
        case "Send":
          this.rightSidebar = Send;
          this.txHash = null;
          break;
        case "Receive":
          this.rightSidebar = Receive;
          this.txHash = null;
          break;
        case "TransactionDetails":
          this.rightSidebar = TransactionDetails;
          break;
        case "Settings":
          this.rightSidebar = AccountSettings;
          break;
      }
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
      this.rightSidebar = null;
      this.txHash = null;
    },
    onTxHash(txHash) {
      this.txHash = txHash;
      this.setRightSidebar("TransactionDetails");
    },
    getButtonClassNames(route) {
      let classNames = ["button"];
      if (route === this.$route.name) {
        classNames.push("active");
      }
      return classNames;
    }
  }
};
</script>

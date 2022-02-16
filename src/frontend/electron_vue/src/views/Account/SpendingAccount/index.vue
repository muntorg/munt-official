<template>
  <div class="spending-account">
    <portal to="header-slot">
      <section class="header flex-row">
        <main-header
          class="info"
          :title="account.label"
          :subtitle="account.balance.toFixed(2) + ' ' + totalBalanceFiat"
        />
        <div
          v-if="!UIConfig.showSidebar"
          style="margin-right: 10px"
          class="button"
          @click="showSettings"
        >
          <fa-icon :icon="['fal', 'cog']" />
        </div>
        <div
          v-if="!UIConfig.showSidebar"
          class="button"
          @click="changeLockSettings"
        >
          <fa-icon :icon="['fal', lockIcon]" />
        </div>
        <div v-if="UIConfig.showSidebar" class="settings flex-col">
          <span class="button" @click="setRightSidebar('Settings')">
            <fa-icon :icon="['fal', 'cog']" />
          </span>
        </div>
      </section>
    </portal>
    <transactions
      v-if="UIConfig.showSidebar"
      :mutations="mutations"
      @tx-hash="onTxHash"
      :tx-hash="txHash"
    />
    <div v-if="!UIConfig.showSidebar">
      <router-view />
    </div>
    <portal to="footer-slot">
      <section class="footer">
        <span
          class="button"
          @click="setRightSidebar('Send')"
          v-if="showSendButton"
        >
          <fa-icon :icon="['fal', 'arrow-from-bottom']" />
          {{ $t("buttons.send") }}
        </span>
        <span
          class="button"
          @click="setRightSidebar('Receive')"
          v-if="showReceiveButton"
        >
          <fa-icon :icon="['fal', 'arrow-to-bottom']" />
          {{ $t("buttons.receive") }}
        </span>
      </section>
    </portal>

    <portal to="sidebar-right">
      <component
        v-if="rightSidebar"
        :is="rightSidebar"
        v-bind="rightSidebarProps"
      />
    </portal>
  </div>
</template>

<script>
import { mapState, mapGetters } from "vuex";
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
      UIConfig: UIConfig
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
    ...mapState("app", ["rate"]),
    ...mapState("wallet", ["mutations", "walletPassword"]),
    ...mapGetters("wallet", ["totalBalance"]),
    lockIcon() {
      return this.walletPassword ? "unlock" : "lock";
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
      return `€ ${(this.account.balance * this.rate).toFixed(2)}`;
    }
  },
  methods: {
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
    }
  }
};
</script>

<style lang="less" scoped>
.spending-account {
  height: 100%;
  padding: 20px 15px 15px 15px;
}
.header {
  align-items: center;

  & > .info {
    width: calc(100% - 26px);
    padding-right: 10px;
  }

  & > .settings {
    font-size: 16px;
    padding: calc((var(--header-height) - 40px) / 2) 0;

    & span {
      padding: 10px;
      cursor: pointer;

      &:hover {
        background: #f5f5f5;
      }
    }
  }
}

.footer {
  text-align: center;
  line-height: calc(var(--footer-height) - 1px);

  & svg {
    font-size: 14px;
    margin-right: 5px;
  }

  & .button {
    display: inline-block;
    padding: 0 20px 0 20px;
    line-height: 32px;
    font-weight: 500;
    font-size: 1em;
    color: var(--primary-color);
    text-align: center;
    cursor: pointer;

    &:hover {
      background-color: #f5f5f5;
    }
  }
}
</style>

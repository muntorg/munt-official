<template>
  <div class="spending-account">
    <portal to="header-slot">
      <section class="header flex-row">
        <main-header
          class="info"
          :title="account.label"
          :subtitle="account.balance.toFixed(2)"
        />
        <div class="settings flex-col">
          <span class="button" @click="setRightSidebar('Settings')">
            <fa-icon :icon="['fal', 'cog']" />
          </span>
        </div>
      </section>
    </portal>

    <mutation-list
      :mutations="mutations"
      @tx-hash="onTxHash"
      :tx-hash="txHash"
    />

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
import { mapState } from "vuex";
import EventBus from "../../../EventBus";

import MutationList from "./MutationList";
import SendGulden from "./SendGulden";
import ReceiveGulden from "./ReceiveGulden";
import TransactionDetails from "./TransactionDetails";
import AccountSettings from "../AccountSettings";

export default {
  name: "SpendingAccount",
  props: {
    account: null
  },
  data() {
    return {
      rightSidebar: null,
      txHash: null
    };
  },
  mounted() {
    EventBus.$on("close-right-sidebar", this.closeRightSidebar);
  },
  beforeDestroy() {
    EventBus.$off("close-right-sidebar", this.closeRightSidebar);
  },
  components: {
    MutationList
  },
  computed: {
    ...mapState("wallet", ["mutations"]),
    showSendButton() {
      return !this.rightSidebar || this.rightSidebar !== SendGulden;
    },
    showReceiveButton() {
      return !this.rightSidebar || this.rightSidebar !== ReceiveGulden;
    },
    rightSidebarProps() {
      if (this.rightSidebar === TransactionDetails) {
        return { txHash: this.txHash };
      }
      if (this.rightSidebar === AccountSettings) {
        return { account: this.account };
      }
      return null;
    }
  },
  methods: {
    setRightSidebar(name) {
      switch (name) {
        case "Send":
          this.rightSidebar = SendGulden;
          this.txHash = null;
          break;
        case "Receive":
          this.rightSidebar = ReceiveGulden;
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
.header {
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

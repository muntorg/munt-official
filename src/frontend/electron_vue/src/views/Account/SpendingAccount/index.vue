<template>
  <div class="spending-account">
    <portal to="header-slot">
      <section class="header flex-row">
        <main-header
          class="info"
          :title="account.label"
          :subtitle="account.balance"
        />
        <div class="settings flex-col" v-if="false /* not implemented yet */">
          <span>
            <fa-icon :icon="['fal', 'cog']" />
          </span>
        </div>
      </section>
    </portal>

    <mutation-list :mutations="mutations" @tx-hash="onTxHash" />

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
import SendNovo from "./SendNovo";
import ReceiveNovo from "./ReceiveNovo";
import TransactionDetails from "./TransactionDetails";

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
    ...mapState(["mutations"]),
    showSendButton() {
      return !this.rightSidebar || this.rightSidebar !== SendNovo;
    },
    showReceiveButton() {
      return !this.rightSidebar || this.rightSidebar !== ReceiveNovo;
    },
    rightSidebarProps() {
      if (this.rightSidebar === TransactionDetails) {
        console.log(this.txHash + "?>?!");
        return { txHash: this.txHash };
      }
      return null;
    }
  },
  methods: {
    setRightSidebar(name) {
      switch (name) {
        case "Send":
          this.rightSidebar = SendNovo;
          break;
        case "Receive":
          this.rightSidebar = ReceiveNovo;
          break;
      }
    },
    closeRightSidebar() {
      this.rightSidebar = null;
    },
    onTxHash(txHash) {
      this.txHash = txHash;
      this.rightSidebar = TransactionDetails;
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

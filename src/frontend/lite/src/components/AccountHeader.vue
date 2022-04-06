<template>
  <div class="account-header">
    <div v-if="!editMode" class="flex-row">
      <div v-if="isSingleAccount" class="flex-row flex-1">
        <div class="logo" />
        <div class="balance-row flex-1">
          <span>{{ balanceForDisplay }}</span>
          <span>{{ totalBalanceFiat }}</span>
        </div>
      </div>
      <div v-else class="left-colum" @click="editName">
        <account-tooltip type="Account" :account="account">
          <div class="flex-row flex-1">
            <div class="accountname ellipsis">{{ name }}</div>
            <fa-icon class="pen" :icon="['fal', 'fa-pen']" />
          </div>
          <div class="balance-row">
            <span>{{ balanceForDisplay }}</span>
            <span>{{ totalBalanceFiat }}</span>
          </div>
        </account-tooltip>
      </div>
      <div v-if="showBuySellButtons">
        <button outlined class="small" @click="buyCoins" :disabled="buyDisabled">{{ $t("buttons.buy") }}</button>
        <button outlined class="small" @click="sellCoins" :disabled="sellDisabled">{{ $t("buttons.sell") }}</button>
      </div>
      <div v-if="isSingleAccount" class="flex-row icon-buttons">
        <div class="icon-button">
          <fa-icon :icon="['fal', 'cog']" @click="showSettings" />
        </div>
        <div class="icon-button">
          <fa-icon :icon="['fal', lockIcon]" @click="changeLockSettings" />
        </div>
      </div>
    </div>
    <input v-else ref="accountNameInput" type="text" v-model="newAccountName" @keydown="onKeydown" @blur="cancelEdit" />
  </div>
</template>

<script>
import { mapState } from "vuex";
import { AccountsController, BackendUtilities } from "../unity/Controllers";
import { formatMoneyForDisplay } from "../util.js";
import AccountTooltip from "./AccountTooltip.vue";
import EventBus from "../EventBus";
import WalletPasswordDialog from "../components/WalletPasswordDialog";

export default {
  components: { AccountTooltip },
  name: "AccountHeader",
  data() {
    return {
      editMode: false,
      newAccountName: null,
      buyDisabled: false,
      sellDisabled: false
    };
  },
  props: {
    account: {
      type: Object,
      default: () => {}
    },
    isSingleAccount: {
      type: Boolean,
      default: false
    }
  },
  computed: {
    ...mapState("app", ["rate"]),
    ...mapState("wallet", ["walletPassword"]),
    name() {
      return this.account ? this.account.label : null;
    },
    totalBalanceFiat() {
      if (!this.rate) return "";
      return `€ ${formatMoneyForDisplay(this.account.balance * this.rate, true)}`;
    },
    balanceForDisplay() {
      if (!this.account || this.account.balance === undefined) return "";
      return formatMoneyForDisplay(this.account.balance);
    },
    showBuySellButtons() {
      return !this.account || (this.account.type === "Desktop" && !this.editMode);
    },
    lockIcon() {
      return this.walletPassword ? "unlock" : "lock";
    }
  },
  watch: {
    name: {
      immediate: true,
      handler() {
        this.editMode = false;
      }
    }
  },
  methods: {
    editName() {
      this.newAccountName = this.name;
      this.editMode = true;
      this.$nextTick(() => {
        this.$refs["accountNameInput"].focus();
      });
    },
    onKeydown(e) {
      switch (e.keyCode) {
        case 13:
          this.changeAccountName();
          break;
        case 27:
          this.editMode = false;
          break;
      }
    },
    changeAccountName() {
      if (this.newAccountName !== this.account.label) {
        AccountsController.RenameAccount(this.account.UUID, this.newAccountName);
      }
      this.editMode = false;
    },
    cancelEdit() {
      this.editMode = false;
    },
    async sellCoins() {
      try {
        this.sellDisabled = true;
        const url = await BackendUtilities.GetSellSessionUrl();
        window.open(url, "sell-coins");
      } finally {
        this.sellDisabled = false;
      }
    },
    async buyCoins() {
      try {
        this.buyDisabled = true;
        const url = await BackendUtilities.GetBuySessionUrl();
        window.open(url, "buy-coins");
      } finally {
        this.buyDisabled = false;
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
    }
  }
};
</script>

<style lang="less" scoped>
.account-header {
  width: 100%;
  height: var(--header-height);
  line-height: var(--header-height);

  & > div {
    align-items: center;
    justify-content: center;
  }
}

.left-colum {
  display: flex;
  flex-direction: column;
  flex: 1;
  white-space: nowrap;
  overflow: hidden;
  height: 40px;
  cursor: pointer;
  position: relative;
}

.balance-row {
  line-height: 20px;
  font-size: 0.9em;

  & :first-child {
    margin-right: 10px;
  }
}

button.small {
  height: 20px !important;
  line-height: 20px !important;
  font-size: 10px !important;
  padding: 0 10px !important;
  margin-left: 5px;
}

.accountname {
  flex: 1;
  font-size: 1em;
  font-weight: 500;
  line-height: 20px;
  margin-right: 30px;
}

.pen {
  display: none;
  position: absolute;
  right: 5px;
  line-height: 20px;
}

.left-colum:hover .pen {
  display: block;
}

.logo {
  width: 22px;
  min-width: 22px;
  height: 22px;
  min-height: 22px;
  background: url("../img/logo.svg"), linear-gradient(transparent, transparent);
  background-size: cover;
  margin-right: 10px;
}

.icon-buttons {
  margin-left: 10px;
}

.icon-button {
  display: inline-block;
  padding: 0 13px;
  line-height: 40px;
  height: 40px;
  font-weight: 500;
  font-size: 1em;
  color: var(--primary-color);
  text-align: center;
  cursor: pointer;

  &:hover {
    background-color: #f5f5f5;
  }
}
</style>

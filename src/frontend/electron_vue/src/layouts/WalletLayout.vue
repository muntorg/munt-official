<template>
  <div class="wallet-layout">
    <div class="sidebar">
      <div class="header">
        <div class="logo" />
        <div class="total-balance">
          {{ totalBalance }}
        </div>
      </div>
      <div class="accounts-section">
        <h4 class="accounts-header">
          accounts
        </h4>
        <div class="accounts-scroller">
          <div class="account-cat">
            <div class="status">
              <fa-icon :icon="['fal', 'chevron-down']" />
            </div>
            <div class="info">
              <div class="title">Spending</div>
              <div class="balance">{{ balanceFor("spending") }}</div>
            </div>
            <div class="add"><fa-icon :icon="['fal', 'plus']" /></div>
          </div>

          <div
            v-for="account in spendingAccounts"
            :key="account.UUID"
            class="account"
            :class="{ active: account.UUID === activeAccount }"
          >
            <router-link
              :to="{ name: 'account', params: { id: account.UUID } }"
            >
              {{ account.label }}
              <span class="balance">{{ account.balance }}</span>
            </router-link>
          </div>

          <div class="account-cat">
            <div class="status">
              <fa-icon :icon="['fal', 'chevron-right']" />
            </div>
            <div class="info">
              <div class="title">Holding</div>
              <div class="balance">{{ balanceFor("holding") }}</div>
            </div>
            <div class="add"><fa-icon :icon="['fal', 'plus']" /></div>
          </div>

          <div
            v-for="account in holdingAccounts"
            :key="account.UUID"
            class="account"
            :class="{ active: account.UUID === activeAccount }"
          >
            <router-link
              :to="{ name: 'account', params: { id: account.UUID } }"
            >
              {{ account.label }}
              <span class="balance">{{ account.balance }}</span>
            </router-link>
          </div>

          <div class="account-cat">
            <div class="status">
              <fa-icon :icon="['fal', 'chevron-right']" />
            </div>
            <div class="info">
              <div class="title">Mining</div>
              <div class="balance">{{ balanceFor("mining") }}</div>
            </div>
            <div class="add"><fa-icon :icon="['fal', 'plus']" /></div>
          </div>

          <div
            v-for="account in miningAccounts"
            :key="account.UUID"
            class="account"
            :class="{ active: account.UUID === activeAccount }"
          >
            <router-link
              :to="{ name: 'account', params: { id: account.UUID } }"
            >
              {{ account.label }}
              <span class="balance">{{ account.balance }}</span>
            </router-link>
          </div>
        </div>
      </div>
      <div class="footer">
        <div class="status"></div>
        <div class="button" @click="changeLockSettings">
          <fa-icon :icon="lockIcon" />
        </div>

        <div class="button" @click="showSettings">
          <fa-icon :icon="['fal', 'user-circle']" />
        </div>
      </div>
    </div>
    <div class="main">
      <router-view />
    </div>
  </div>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import EventBus from "../EventBus";

import PasswordDialog from "../components/PasswordDialog";

export default {
  name: "WalletLayout",
  computed: {
    ...mapState(["activeAccount", "password"]),
    ...mapGetters(["totalBalance", "accounts"]),
    spendingAccounts() {
      return this.accounts.filter(
        x => x.type === "Desktop" /* || x.type === "Mobile" etc. */
      );
    },
    holdingAccounts() {
      return this.accounts.filter(x => x.type === "Witness");
    },
    miningAccounts() {
      return this.accounts.filter(x => x.type === "Mining");
    },
    lockIcon() {
      return this.password ? ["fal", "unlock"] : ["fal", "lock"];
    }
  },
  methods: {
    balanceFor(type) {
      let accounts = [];
      switch (type) {
        case "spending":
          accounts = this.spendingAccounts;
          break;
        case "holding":
          accounts = this.holdingAccounts;
          break;
        case "mining":
          accounts = this.miningAccounts;
          break;
      }
      return accounts.reduce(function(acc, obj) {
        return acc + obj.balance;
      }, 0);
    },
    showSettings() {
      if (this.$route.path.indexOf("/settings/") === 0) return;
      this.$router.push({ name: "settings" });
    },
    changeLockSettings() {
      if (this.password) {
        this.$store.dispatch({ type: "SET_PASSWORD", password: null });
      } else {
        EventBus.$emit("show-dialog", {
          title: this.$t("password_dialog.unlock_wallet"),
          component: PasswordDialog,
          showButtons: false
        });
      }
    }
  }
};
</script>

<style lang="less" scoped>
.wallet-layout {
  --sidebar-width: 240px;

  --sidebar-background-color: #000;
  --sidebar-color: #ccc;
  --sidebar-border-color: #333;

  --main-border-color: #ddd;

  width: 100%;
  height: 100vh;
  overflow: hidden;

  display: flex;
  flex-direction: row;

  & .sidebar {
    width: var(--sidebar-width);
    background-color: var(--sidebar-background-color);
    color: var(--sidebar-color);

    & .header {
      height: var(--header-height);
      border-bottom: 1px solid var(--sidebar-border-color);

      padding: 20px;
      color: #fff;
      display: flex;

      & .logo {
        width: 22px;
        height: 22px;
        background: url("../img/logo.svg"),
          linear-gradient(transparent, transparent);
        background-size: cover;
      }

      & .total-balance {
        padding: 0 0 0 10px;
        line-height: 22px;
      }
    }

    & .accounts-section {
      height: calc(100% - var(--header-height) - var(--footer-height));
      padding: 24px 0 0 0;

      & .accounts-header {
        padding: 0 20px 0 20px;
        height: 20px;
        margin-bottom: 4px;
      }

      & .accounts-scroller {
        height: calc(100% - 20px - 4px);
        overflow: hidden;

        --scrollbarBG: #030303;
        --thumbBG: #ccc;

        &:hover {
          overflow-y: overlay;
        }
        &::-webkit-scrollbar {
          width: 11px;
        }
        &::-webkit-scrollbar-track {
          background: var(--scrollbarBG);
        }
        &::-webkit-scrollbar-thumb {
          border: 4px solid var(--scrollbarBG);
          background-color: var(--thumbBG);
          border-radius: 11px;
        }
      }

      & .account-cat {
        display: flex;
        padding: 12px 0 12px 0;

        // why highlight the category on hover?
        // &:hover {
        //   background: #222;
        // }

        & .status {
          width: 44px;
          padding: 0 0 0 20px;
          font-size: 12px;
          line-height: 16px;
        }

        & .title {
          line-height: 16px;
          font-size: 1em;
          font-weight: 500;
          margin: 0 0 6px 0;
        }

        & .info {
          flex-grow: 1;
        }

        & .balance {
          line-height: 10px;
          font-size: 0.8em;
          font-weight: 500;
          letter-spacing: 0.02em;
          text-transform: uppercase;
        }

        & .add {
          margin: 0 24px 0 0;
          line-height: 16px;
          font-size: 16px;
        }
      }

      & .account a {
        display: flex;
        flex-direction: column;
        padding: 8px 24px 8px 25px;
        font-size: 1em;
        color: #ccc;
        line-height: 16px;
        cursor: pointer;
      }

      & .account a:hover {
        background-color: #222;
      }

      & .account.active a {
        font-weight: 500;
        color: #fff;
        background-color: #009572;
      }

      & .account .balance {
        margin: 6px 0 0 0;
        font-size: 0.8em;
        line-height: 10px;
      }
    }

    & .footer {
      height: var(--footer-height);
      border-top: 1px solid var(--sidebar-border-color);
      font-size: 16px;
      font-weight: 400;

      display: flex;
      flex-direction: row;

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
  }

  & .main {
    flex: 1;
  }
}
</style>

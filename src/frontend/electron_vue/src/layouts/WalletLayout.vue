<template>
  <div class="wallet-layout">
    <div class="sidebar">
      <header>
        <div class="logo" />
        <div class="balance-total">
          {{ totalBalance }}
        </div>
      </header>
      <section>
        <h4 class="accounts-header">
          accounts
        </h4>
        <div class="accounts">
          <div class="account-cat">
            <div class="status">
              <fa-icon :icon="['fal', 'chevron-down']" />
            </div>
            <div class="symbol">
              <fa-icon :icon="['fal', 'credit-card']" />
            </div>
            <div class="info">
              <div class="title">spending</div>
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
            <router-link to="/account">
              {{ account.label }}
              <span class="balance">{{ account.balance }}</span>
            </router-link>
          </div>
        </div>
      </section>
      <footer></footer>
    </div>
    <div class="main">
      <header></header>
      <section>
        <router-view />
      </section>
      <footer></footer>
    </div>
    <div class="transfer"></div>
  </div>
</template>

<script>
import { mapState, mapGetters } from "vuex";

export default {
  name: "WalletLayout",
  computed: {
    ...mapState(["activeAccount"]),
    ...mapGetters(["totalBalance", "accounts"]),
    spendingAccounts() {
      return this.accounts.filter(
        x =>
          x.type === "Desktop" /* || x.type === "Mobile" etc. -> sort by name */
      );
    }
  },
  methods: {
    balanceFor(type) {
      let accounts = [];
      switch (type) {
        case "spending":
          accounts = this.spendingAccounts;
          break;
      }
      return accounts.reduce(function(acc, obj) {
        return acc + obj.balance;
      }, 0);
    }
  }
};
</script>

<style lang="less" scoped>
.wallet-layout {
  --sidebar-width: 240px;
  --transfer-width: 400px;
  --header-height: 62px;
  --footer-height: 53px;

  --sidebar-background-color: #000;
  --sidebar-color: #ccc;
  --sidebar-border-color: #333;
  --main-border-color: #ddd;

  width: 100%;
  height: 100vh;
  overflow: hidden;

  display: grid;
  grid-template-columns: var(--sidebar-width) calc(100% - var(--sidebar-width));
  grid-template-areas:
    "sidebar-header main-header transfer"
    "sidebar-content main-content transfer"
    "sidebar-footer main-footer transfer";

  &.transfer-open {
    grid-template-columns:
      var(--sidebar-width) calc(
        100% - var(--sidebar-width) - var(--transfer-width)
      )
      var(--transfer-width);
  }
}

header {
  height: var(--header-height);
}

section {
  height: calc(100vh - var(--header-height) - var(--footer-height));
}

footer {
  height: var(--footer-height);
}

.sidebar {
  color: var(--sidebar-color);
  background-color: var(--sidebar-background-color);
  border-right: 1px solid var(--sidebar-border-color);

  & header {
    grid-area: "sidebar-header";
    border-bottom: 1px solid var(--sidebar-border-color);
  }

  & section {
    grid-area: "sidebar-content";
  }

  & footer {
    grid-area: "sidebar-footer";
    border-top: 1px solid var(--sidebar-border-color);
  }
}

.main {
  & header {
    grid-area: "main-header";
    border-bottom: 1px solid var(--main-border-color);
  }

  & section {
    grid-area: "main-content";
  }

  & footer {
    grid-area: "main-footer";
    border-top: 1px solid var(--main-border-color);
  }
}

.transfer {
  padding: 0 30px 10px 30px;
  background-color: #f5f5f5;
}

// ==================================================

.sidebar header {
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

  & .balance-total {
    padding: 0 0 0 10px;
    line-height: 22px;
  }
}

.sidebar section {
  padding: 30px 0 0 0;

  & .accounts-header {
    padding: 0 20px 0 20px;
    height: 20px;
    margin-bottom: 4px;
  }

  & .accounts {
    height: calc(100% - 20px - 4px);
  }

  & .account-cat {
    display: flex;
    padding: 17px 0 17px 0;

    &:hover {
      background: #222;
    }

    & .status {
      padding: 5px 10px 0 20px;
      font-size: 12px;
    }

    & .symbol {
      font-size: 16px;
      padding: 0 7px 0 0;
    }

    & .title {
      line-height: 0.9em;
      font-size: 0.9em;
      font-weight: 600;
      letter-spacing: 0.02em;
      text-transform: uppercase;
      padding: 0 0 6px 0;
    }

    & .info {
      flex-grow: 1;
    }

    & .balance {
      line-height: 0.8em;
      font-size: 0.8em;
      font-weight: 500;
      letter-spacing: 0.02em;
      text-transform: uppercase;
    }

    & .add {
      margin: 0 25px 0 0;
      line-height: 26px;
      font-size: 16px;
    }
  }

  & .account a {
    display: flex;
    flex-direction: column;
    padding: 10px 20px 10px 40px;
    font-size: 0.9em;
    color: #ccc;
    line-height: 1.2em;
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
    font-size: 0.9em;
  }
}
</style>

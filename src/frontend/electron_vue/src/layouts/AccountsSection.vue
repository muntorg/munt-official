<template>
  <section class="accounts">
    <h4>{{ $t("accounts_section.accounts") }}</h4>
    <section class="scrollable dark">
      <div v-for="category in categories" :key="category">
        <div class="category flex-row">
          <div class="toggle" @click="toggleCategory(category)">
            <fa-icon :icon="['fal', getCategoryToggleIcon(category)]" />
          </div>
          <div class="info" @click="toggleCategory(category)">
            <div class="title ellipsis">
              {{ $t(`accounts_section.categories.${category}`) }}
            </div>
            <div class="balance">{{ getBalanceFor(category) }}</div>
          </div>
          <div class="add">
            <div class="button" @click="addAccountFor(category)">
              <fa-icon :icon="['fal', 'plus']" />
            </div>
          </div>
        </div>
        <div class="account active" v-if="showNewAccountFor(category) && false">
          <div>New account</div>
          <div class="balance">0</div>
        </div>
        <div v-if="opened[category]">
          <div
            v-for="account in getAccountsFor(category)"
            :key="account.UUID"
            class="account"
            :class="accountClass(account.UUID)"
          >
            <router-link
              :disabled="isActiveAccount(account.UUID)"
              :event="!isActiveAccount(account.UUID) ? 'click' : ''"
              class="flex-col"
              :to="{ name: 'account', params: { id: account.UUID } }"
            >
              <span class="ellipsis">{{ account.label }}</span>
              <span class="balance">{{
                displayBalanceForAccount(account)
              }}</span>
            </router-link>
          </div>
        </div>
      </div>
    </section>
  </section>
</template>

<script>
import { mapState, mapGetters } from "vuex";
import { formatMoneyForDisplay } from "../util.js";

export default {
  name: "AccountsSection",
  data() {
    return {
      categories: ["spending", "holding"],
      opened: {
        spending: false,
        holding: false
      }
    };
  },
  computed: {
    ...mapState("wallet", ["activeAccount"]),
    ...mapState("app", ["coreReady"]),
    ...mapGetters("wallet", ["accounts"]),
    activeCategory() {
      if (this.activeAccount === null) return null;
      let account = this.accounts.find(x => x.UUID === this.activeAccount);
      if (account === undefined) return null;
      switch (account.type) {
        case "Desktop":
          return "spending";
        case "Holding":
          return "holding";
      }
      return null;
    }
  },
  watch: {
    activeCategory() {
      this.toggleCategory(this.activeCategory, true);
    }
  },
  methods: {
    isActiveAccount(accountUUID) {
      return (
        this.$route.path.indexOf("/account") == 0 &&
        accountUUID === this.activeAccount
      );
    },
    accountClass(accountUUID) {
      return this.$route.path.indexOf("/account/") === 0 &&
        accountUUID === this.activeAccount
        ? "active"
        : "";
    },
    displayBalanceForAccount(account) {
      return formatMoneyForDisplay(account.balance);
    },
    getAccountsFor(category) {
      let types;
      switch (category) {
        case "spending":
          types = ["Desktop"];
          break;
        case "holding":
          types = ["Holding"];
          break;
      }
      if (types === undefined) return [];
      return this.accounts.filter(
        x => x.state === "Normal" && types.indexOf(x.type) !== -1
      );
    },
    getCategoryToggleIcon(category) {
      return this.opened[category] ? "chevron-down" : "chevron-right";
    },
    toggleCategory(category, open) {
      if (this.categories.indexOf(category) === -1) return;
      this.opened[category] = open || !this.opened[category];
    },
    getBalanceFor(category) {
      let accounts = this.getAccountsFor(category);
      return formatMoneyForDisplay(
        accounts.reduce(function(acc, obj) {
          return acc + obj.balance;
        }, 0)
      );
    },
    showNewAccountFor(category) {
      switch (category) {
        case "holding":
          return this.$route.name === "add-holding-account";
        case "spending":
          return this.$route.name === "add-spending-account";
        default:
          return false;
      }
    },
    addAccountFor(category) {
      switch (category) {
        case "holding":
          this.$router.push({ name: "add-holding-account" });
          break;
        case "spending":
          this.$router.push({ name: "add-spending-account" });
          break;
        default:
          console.log(`add account for ${category} not implemented yet`);
          break;
      }
    }
  }
};
</script>

<style lang="less" scoped>
.accounts {
  height: 100%;

  & h4 {
    padding: 20px 20px 0 20px;
    margin-bottom: 5px;
    font-weight: 500;
    font-size: 0.8em;
  }

  & > .scrollable {
    height: calc(100% - 48px);
  }
}

.category {
  padding: 10px 0;

  & > .toggle {
    width: 44px;
    padding: 0 0 0 20px;
    font-size: 12px;
    line-height: 16px;
    cursor: pointer;

    &.hide {
      visibility: hidden;
    }
  }

  & > .info {
    width: 150px;
    margin-right: 10px;
    cursor: pointer;

    & > .title {
      line-height: 16px;
      font-size: 0.85em;
      font-weight: 600;
      letter-spacing: 0.02em;
      text-transform: uppercase;
    }

    & > .balance {
      margin: 4px 0 1px 0;
      line-height: 12px;
      font-size: 0.8em;
      font-weight: 500;
      letter-spacing: 0.02em;
      text-transform: uppercase;
    }
  }

  & > .add {
    flex: 1;
    text-align: right;
    margin: 0 24px 0 0;
    line-height: 16px;
    font-size: 16px;
    cursor: pointer;

    & > .button:hover {
      padding: 4px 5px;
      margin: -4px -5px;
      background: #333;
    }
  }
}

.category.empty {
  & > .toggle {
    visibility: hidden;
  }

  & > .info {
    cursor: default;
  }
}

.account {
  padding: 8px 24px;
  font-size: 1em;
  color: #ccc;
  line-height: 16px;

  &:hover {
    background-color: var(--account-background-color--hover);
  }

  & a {
    color: #ccc;
    cursor: pointer;
  }

  &.active {
    color: #fff;
    background-color: var(--account-background-color--active);

    & a {
      color: #fff;
    }
  }

  & .balance {
    margin: 4px 0 1px 0;
    font-size: 0.8em;
    line-height: 12px;
  }
}
</style>

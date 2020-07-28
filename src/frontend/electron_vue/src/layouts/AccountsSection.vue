<template>
  <section class="accounts">
    <h4>{{ $t("accounts_section.accounts") }}</h4>
    <section class="scrollable dark">
      <div v-for="category in categories" :key="category">
        <div class="category flex-row" :class="getCategoryClass(category)">
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
        <div v-if="opened[category]">
          <div
            v-for="account in getAccountsFor(category)"
            :key="account.UUID"
            class="account"
            :class="{ active: account.UUID === activeAccount }"
          >
            <router-link
              class="flex-col"
              :to="{ name: 'account', params: { id: account.UUID } }"
            >
              <span class="ellipsis">{{ account.label }}</span>
              <span class="balance">{{ account.balance }}</span>
            </router-link>
          </div>
        </div>
      </div>
    </section>
  </section>
</template>

<script>
import { mapState, mapGetters } from "vuex";

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
    ...mapState(["activeAccount"]),
    ...mapGetters(["accounts"]),
    activeCategory() {
      if (this.activeAccount === null) return null;
      let account = this.accounts.find(x => x.UUID === this.activeAccount);
      if (account === undefined) return null;
      switch (account.type) {
        case "Desktop":
          return "spending";
        case "Witness":
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
    getAccountsFor(category) {
      let types;
      switch (category) {
        case "spending":
          types = ["Desktop"];
          break;
        case "holding":
          types = ["Witness"];
          break;
      }
      if (types === undefined) return [];
      return this.accounts.filter(
        x => x.state === "Normal" && types.indexOf(x.type) !== -1
      );
    },
    getCategoryClass(category) {
      return this.getAccountsFor(category).length === 0 ? "empty" : "";
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
      return accounts
        .reduce(function(acc, obj) {
          return acc + obj.balance;
        }, 0)
        .toFixed(2);
    },
    addAccountFor(category) {
      console.log(`todo: add account for ${category}`);
    }
  }
};
</script>

<style lang="less" scoped>
.accounts {
  height: calc(100% - var(--header-height) - var(--footer-height));

  & h4 {
    padding: 24px 20px 0 20px;
    margin-bottom: 8px;
    font-weight: 500;
    font-size: 0.8em;
  }

  & > .scrollable {
    height: calc(100% - 24px - 20px - 4px);
  }
}

.category {
  padding: 12px 0;

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
      margin: 0 0 6px 0;
    }

    & > .balance {
      line-height: 10px;
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
  & a {
    padding: 8px 20px;
    font-size: 1em;
    color: #ccc;
    line-height: 16px;
    cursor: pointer;

    &:hover {
      background-color: #222;
    }
  }

  &.active a {
    font-weight: 500;
    color: #fff;
    background-color: #009572;
  }

  & .balance {
    margin: 6px 0 0 0;
    font-size: 0.8em;
    line-height: 10px;
  }
}
</style>

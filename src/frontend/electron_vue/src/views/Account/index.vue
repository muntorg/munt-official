<template>
  <component :is="accountType" :account="account" />
</template>

<script>
import { mapGetters } from "vuex";

import SpendingAccount from "./SpendingAccount";
import HoldingAccount from "./HoldingAccount";
import MiningAccount from "./MiningAccount";

export default {
  name: "Account",
  data() {
    return {
      accountType: "div"
    };
  },
  props: {
    id: null
  },
  components: {
    SpendingAccount,
    HoldingAccount,
    MiningAccount
  },
  computed: {
    ...mapGetters("wallet", ["account"])
  },
  watch: {
    account() {
      this.onAccountChanged();
    }
  },
  mounted() {
    this.onAccountChanged();
  },
  methods: {
    onAccountChanged() {
      if (this.account) {
        switch (this.account.type) {
          case "Desktop":
            this.accountType = SpendingAccount;
            break;
          case "Holding":
            this.accountType = HoldingAccount;
            break;
          case "Mining":
            this.accountType = MiningAccount;
            break;
          default:
            this.accountType = "div";
            break;
        }
      }
    }
  }
};
</script>

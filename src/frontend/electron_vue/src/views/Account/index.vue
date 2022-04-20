<template>
  <component :is="accountType" :account="account" />
</template>

<script>
import { mapGetters } from "vuex";

import SpendingAccount from "./SpendingAccount";
import SavingAccount from "./SavingAccount";
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
    SavingAccount,
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
          case "Witness":
            this.accountType = SavingAccount;
            break;
          case "Mining":
            this.accountType = MiningAccount;
            break;
          default:
            this.accountType = "div";
            break;
        }
      }
      // remove the activity indicator at this point
      this.$store.dispatch("app/SET_ACTIVITY_INDICATOR", false);
    }
  }
};
</script>

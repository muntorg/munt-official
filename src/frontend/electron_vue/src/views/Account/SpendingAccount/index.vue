<template>
  <div class="spending-account">
    <portal to="header-slot">
      <account-header :account="account"></account-header>
    </portal>

    <transactions v-if="isAccountView" :mutations="mutations" @tx-hash="onTxHash" :tx-hash="txHash" />

    <router-view />

    <portal to="footer-slot">
      <div style="display: flex; justify-content: center">
        <footer-button title="buttons.transactions" :icon="['far', 'list-ul']" routeName="account" @click="routeTo" />
        <footer-button title="buttons.send" :icon="['fal', 'arrow-from-bottom']" routeName="send" @click="routeTo" />
        <footer-button title="buttons.receive" :icon="['fal', 'arrow-to-bottom']" routeName="receive" @click="routeTo" />
      </div>
    </portal>
  </div>
</template>

<script>
import { mapState } from "vuex";
import Transactions from "./Transactions";

export default {
  name: "SpendingAccount",
  props: {
    account: null
  },
  data() {
    return {
      txHash: null
    };
  },
  components: {
    Transactions
  },
  computed: {
    ...mapState("wallet", ["mutations"]),
    isAccountView() {
      return this.$route.name === "account";
    }
  },
  methods: {
    routeTo(route) {
      if (this.$route.name === route) return;
      this.$router.push({ name: route, params: { id: this.account.UUID } });
    },
    onTxHash(txHash) {
      this.txHash = txHash;
    }
  }
};
</script>

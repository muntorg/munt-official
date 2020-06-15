<template>
  <div id="wallet">
    <div class="section">
      <h4>{{ $t("balance.your_address") }}</h4>
      <div class="select-all">{{ receiveAddress }}</div>
      <button class="btn" @click="receiveNovo">{{ $t("wallet.receive_aquired_novo") }}</button>
      <button class="btn" @click="viewPhrase">{{ "View recovery phrase" }}</button>
      <button class="btn" @click="changePassword">{{ "Change password" }}</button>
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";

export default {
  computed: {
    ...mapState(["receiveAddress"]),
    receiveUrl() {
      return `https://novocurrency.com/transfer?receive_address=${this.receiveAddress}`;
    }
  },
  methods: {
    receiveNovo() {
      window.open(this.receiveUrl, "_blank");
    },
    viewPhrase() {
      this.$router.push({ name: "phrase" }); // maybe also route from backend
    },
    changePassword() {
      this.$router.push({ name: "changepassword" }); // maybe also route from backend
    }
  }
};
</script>

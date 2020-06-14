<template>
  <div id="wallet">
    <div class="section">
      <h4>{{ $t("balance.your_address") }}</h4>
      <div>{{ receiveAddress }}</div>
      <button class="btn" @click="receiveNovo">Receive your acquired Novo</button>
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
    }
  }
};
</script>

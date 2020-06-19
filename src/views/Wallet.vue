<template>
  <div id="wallet">
    <div class="section">
      <h4>{{ $t("balance.your_address") }}</h4>
      <div class="address">{{ receiveAddress }} <span class="copy"><fa-icon :icon="['fal', 'long-arrow-right']" /></span></div>
      <button class="btn" @click="receiveNovo">{{ $t("wallet.receive_aquired_novo") }}</button>
    </div>
  </div>
</template>

<style scoped>
.address {
  position: relative;
  float: left;
  width: 100%;
  margin: 0 0 20px 0;
  padding: 0 10px 0 10px;
  font-size: .9em;
  line-height: 38px;
  border: 1px solid #ccc;
  user-select: text;
}

.copy {
  position: absolute;
  top: 0;
  right: 0;
  width: 40px;
  text-align: center;
}

.copy:hover {
    background-color: #f5f5f5;
}
</style>

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

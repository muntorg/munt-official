<template>
  <div id="wallet">
    <div class="section">
      <h4>{{ $t("wallet.your_address") }}</h4>
      <div class="address" @click="copyAddress">
        {{ receiveAddress }}
        <span class="copy"><fa-icon :icon="['fal', 'copy']"/></span>
      </div>
      <button class="btn" @click="receiveNovo">
        {{ $t("wallet.receive_acquired_novo") }}
      </button>
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
  font-size: 0.9em;
  line-height: 38px;
  height: 38px;
  border: 1px solid #ccc;
}

.copy {
  position: absolute;
  top: 0;
  right: 0;
  width: 40px;
  text-align: center;
  cursor: pointer;
}

.copy:hover {
  background-color: #f5f5f5;
}
.address:active,
.copy:active {
  background-color: #009572;
  color: #fff;
}
</style>

<script>
import { clipboard } from "electron";
import { mapState } from "vuex";

export default {
  data() {
    return {
      copyActive: false
    };
  },
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
    copyAddress() {
      this.copyActive = true;
      clipboard.writeText(this.receiveAddress);
      setTimeout(() => {
        this.copyActive = false;
      }, 500);
    }
  }
};
</script>

<template>
  <div class="wallet-view">
    <h4>{{ $t("wallet.your_address") }}</h4>
    <div class="address" @click="copyAddress">
      {{ receiveAddress }}
      <span class="copy"><fa-icon :icon="['fal', 'copy']"/></span>
    </div>

    <novo-button-section>
      <template v-slot:left>
        <button @click="receiveNovo">
          {{ $t("wallet.receive_acquired_novo") }}
        </button>
        <button @click="sendNovo">
          {{ $t("wallet.send_novo") }}
        </button>
      </template>
    </novo-button-section>
  </div>
</template>

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
    sendNovo() {
      this.$router.push({ name: "send" });
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

<style scoped lang="less">
.address {
  position: relative;
  float: left;
  width: 100%;
  margin: 0 0 20px 0;
  padding: 0 10px 0 10px;
  font-size: 0.9em;
  line-height: 38px;
  height: 40px;
  border: 1px solid #ccc;
  user-select: text;
}

.copy {
  position: absolute;
  top: 0;
  right: 0;
  width: 40px;
  line-height: 38px;
  text-align: center;
  cursor: pointer;
}

.address:active,
.copy:active {
  color: #009572;
  border-color: #009572;
}
</style>

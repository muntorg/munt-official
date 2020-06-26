<template>
  <div id="wallet">
    <div class="section">
      <h4>{{ $t("wallet.your_address") }}</h4>
      <div class="address" @click="copyAddress">
        {{ receiveAddress }}
        <span class="copy"><fa-icon :icon="['fal', 'copy']"/></span>
      </div>
      <div class="button-wrapper">
        <novo-button class="btn" @click="receiveNovo">
          {{ $t("wallet.receive_acquired_novo") }}
        </novo-button>
      </div>
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

.button-wrapper {
  margin: 10px 0 0 0;
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

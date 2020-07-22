<template>
  <div class="receive-novo">
    <div class="main">
      <h4>{{ $t("receive_novo.your_address") }}</h4>
      <div class="qr" @click="copyAddress">
        <vue-qrcode
          class="qrcode"
          :width="288"
          :margin="0"
          :value="receiveAddress"
        />
        <div class="address">{{ receiveAddress }}</div>
      </div>
    </div>
    <button @click="receiveNovo">
      {{ $t("receive_novo.receive_acquired_novo") }}
    </button>
  </div>
</template>

<script>
import { mapState } from "vuex";
import { clipboard } from "electron";
import VueQrcode from "vue-qrcode";

export default {
  name: "ReceiveNovo",
  components: {
    VueQrcode
  },
  computed: {
    ...mapState(["receiveAddress"])
  },
  methods: {
    receiveNovo() {
      window.open(
        `https://novocurrency.com/transfer?receive_address=${this.receiveAddress}`,
        "_blank"
      );
    },
    copyAddress() {
      clipboard.writeText(this.receiveAddress);
    }
  }
};
</script>

<style lang="less" scoped>
.receive-novo {
  height: 100%;

  display: flex;
  flex-direction: column;

  & .main {
    flex: 1;

    & .qr {
      background-color: #fff;
      text-align: center;
      cursor: pointer;
    }

    & .qrcode {
      padding: 26px;
    }

    & .address {
      font-size: 0.85em;
      line-height: 32px;
      user-select: all !important;
    }
  }
}

button {
  width: 100%;
}
</style>

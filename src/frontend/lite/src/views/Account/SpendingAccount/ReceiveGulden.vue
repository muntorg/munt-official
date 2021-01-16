<template>
  <div class="receive-gulden flex-col">
    <portal to="sidebar-right-title">
      {{ $t("buttons.receive") }}
    </portal>

    <div class="main">
      <h4>{{ $t("receive_gulden.your_address") }}</h4>
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
    <button @click="buyOrReceiveGulden">
      {{ $t("receive_gulden.buy_or_receive_gulden") }}
    </button>
  </div>
</template>

<script>
import { mapState } from "vuex";
import { clipboard } from "electron";
import VueQrcode from "vue-qrcode";

export default {
  name: "ReceiveGulden",
  components: {
    VueQrcode
  },
  computed: {
    ...mapState("wallet", ["receiveAddress"])
  },
  methods: {
    buyOrReceiveGulden() {
      window.open(
        `https://gulden.com/transfer?receive_address=${this.receiveAddress}`,
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
.receive-gulden {
  height: 100%;

  & .main {
    flex: 1;

    & .qr {
      background-color: #fff;
      text-align: center;
      cursor: pointer;
    }

    & .qrcode {
      width: 100%;
      max-width: 300px;
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

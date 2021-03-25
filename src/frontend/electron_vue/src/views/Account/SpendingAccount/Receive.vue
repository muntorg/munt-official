<template>
  <div class="receive-view flex-col">
    <portal to="sidebar-right-title">
      {{ $t("buttons.receive") }}
    </portal>

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
        <button @click="buyNovo" class="buy-novo" :disabled="buyDisabled">
          {{ $t("buttons.buy_novo") }}
        </button>

  </div>
</template>

<script>
import { mapState } from "vuex";
import { clipboard } from "electron";
import VueQrcode from "vue-qrcode";
import { BackendUtilities } from "@/unity/Controllers";

export default {
  name: "Receive",
  components: {
    VueQrcode
  },
  data() {
    return {
      buyDisabled: false
    };
  },
  computed: {
    ...mapState("wallet", ["receiveAddress"])
  },
  methods: {
    async buyNovo() {
      try {
        this.buyDisabled = true;
        let url = await BackendUtilities.GetBuySessionUrl();
        if (!url) {
          url = "https://novocurrency.com/buy";
        }
        window.open(url, "buy-novo");
      } finally {
        this.buyDisabled = false;
      }
    },
    copyAddress() {
      clipboard.writeText(this.receiveAddress);
    }
  }
};
</script>

<style lang="less" scoped>
.receive-view {
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

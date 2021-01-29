<template>
  <div class="receive-view flex-col">
    <h2>{{ $t("receive_gulden.your_address") }}</h2>
    <p class="information">{{ $t("receive_gulden.information") }}</p>
    <div class="qr">
      <vue-qrcode
        class="qrcode"
        :width="288"
        :margin="0"
        :value="receiveAddress"
      />
      <div class="address-row flex-row">
        <div class="flex-1" />
        <clipboard-field
          class="address"
          :value="receiveAddress"
          confirmation="receive_gulden.address_copied_to_clipboard"
        ></clipboard-field>
        <div class="flex-1" />
      </div>
    </div>
    <div class="flex-1" />
    <gulden-button-section>
      <template v-slot:middle>
        <button @click="buyGulden" class="buy-gulden">
          {{ $t("buttons.buy_gulden") }}
        </button>
      </template>
    </gulden-button-section>
  </div>
</template>

<script>
import { mapState } from "vuex";
import VueQrcode from "vue-qrcode";

export default {
  name: "Receive",
  components: {
    VueQrcode
  },
  computed: {
    ...mapState("wallet", ["receiveAddress"])
  },
  methods: {
    buyGulden() {
      window.open("https://gulden.com/#buy", "buy-gulden");
    }
  }
};
</script>

<style lang="less" scoped>
.receive-view {
  height: 100%;
  text-align: center;
  & .information {
    margin: 0 0 20px 0;
  }
  & .qr {
    background-color: #fff;
    text-align: center;
  }
  & .qrcode {
    width: 100%;
    max-width: 160px;
  }
  & .address-row {
    width: 100%;
    text-align: center;
  }
  & .address {
    font-weight: 500;
    font-size: 0.9em;
    line-height: 32px;
  }

  & .buy-gulden {
    width: 100%;
  }
}
</style>

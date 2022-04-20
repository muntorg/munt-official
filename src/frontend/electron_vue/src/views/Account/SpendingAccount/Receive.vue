<template>
  <div class="receive-view flex-col">
    <div class="main">
      <content-wrapper heading="receive_coins.your_address" content="receive_coins.information"> </content-wrapper>

      <div class="qr" @click="copyQr">
        <vue-qrcode ref="qrcode" class="qrcode" :width="280" :margin="0" :value="receiveAddress" :color="{ dark: '#000000', light: '#ffffff' }" />
      </div>
    </div>
    <div class="address-row flex-row">
      <div class="flex-1" />
      <clipboard-field class="address" :value="receiveAddress" confirmation="receive_coins.address_copied_to_clipboard"></clipboard-field>
      <div class="flex-1" />
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";
import VueQrcode from "vue-qrcode";
import { clipboard, nativeImage } from "electron";
import UIConfig from "../../../../ui-config.json";
import ContentWrapper from "../../../components/layout/ContentWrapper.vue";

export default {
  name: "Receive",
  components: {
    VueQrcode,
    ContentWrapper
  },
  data() {
    return {
      UIConfig: UIConfig
    };
  },
  computed: {
    ...mapState("wallet", ["receiveAddress"])
  },
  methods: {
    copyQr() {
      let img = nativeImage.createFromDataURL(this.$refs.qrcode.$el.src);
      clipboard.writeImage(img);
    }
  }
};
</script>

<style lang="less" scoped>
.receive-view {
  height: 100%;
  text-align: center;

  & .information {
    margin: 0 0 30px 0;
  }
  & .qr {
    text-align: center;
    cursor: pointer;
    margin: 0 auto;
  }
  & .qrcode {
    width: 100%;
    max-width: 140px;
  }
  & .address-row {
    width: 100%;
    text-align: center;
  }
  & .address {
    margin: 5px 0 0 0;
    font-weight: 500;
    font-size: 1em;
    line-height: 1.4em;
  }
  & .buy-coins {
    width: 100%;
  }
}
</style>

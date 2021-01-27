<template>
  <div class="receive-view">
    <h2>{{ $t("receive_gulden.your_address") }}</h2>
    <div class="information">
      {{ $t("receive_gulden.information") }}
    </div>
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
  }
};
</script>

<style lang="less" scoped>
.receive-view {
  text-align: center;

  & .qr {
    margin: 10px 0 0 0;
    background-color: #fff;
    text-align: center;
  }

  & .qrcode {
    width: 100%;
    max-width: 248px;
    padding: 16px;
  }

  & .address-row {
    width: 100%;
    margin: -10px 0 0 0;
    text-align: center;
  }

  & .address {
    font-weight: 500;
    font-size: 0.85em;
    line-height: 32px;
  }
}
</style>

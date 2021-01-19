<template>
  <div class="receive-view">
    <h4>{{ $t("receive_gulden.your_address") }}</h4>
    <div class="qr" @click="copyAddress">
      <vue-qrcode
        class="qrcode"
        :width="288"
        :margin="0"
        :value="receiveAddress"
      />
      <div class="address-row">
        <div class="spacer"></div>
        <div class="address">
          <span v-if="confirmCopy">
            {{ $t("receive_gulden.address_copied_to_clipboard") }}
          </span>
          <span v-else>
            {{ receiveAddress }}
            <fa-icon :icon="['fal', 'copy']" class="copy" />
          </span>
        </div>
        <div class="spacer"></div>
      </div>
    </div>
  </div>
</template>

<script>
import { mapState } from "vuex";
import { clipboard } from "electron";
import VueQrcode from "vue-qrcode";

export default {
  name: "Receive",
  data() {
    return {
      confirmCopy: false
    };
  },
  components: {
    VueQrcode
  },
  computed: {
    ...mapState("wallet", ["receiveAddress"])
  },
  methods: {
    copyAddress() {
      clipboard.writeText(this.receiveAddress);
      this.confirmCopy = true;
      setTimeout(() => {
        this.confirmCopy = false;
      }, 1500);
    }
  }
};
</script>

<style lang="less" scoped>
.receive-view {
  text-align: center;

  & .qr {
    background-color: #fff;
    text-align: center;
    cursor: pointer;
  }

  & .qrcode {
    width: 100%;
    max-width: 248px;
    padding: 16px;
  }

  & .address-row {
    display: flex;
    flex-direction: row;
  }

  & .spacer {
    flex: 1;
  }

  & .address {
    flex: 0 0 300px;
    font-size: 0.85em;
    line-height: 32px;
  }
  & .address:hover {
    color: #0039cc;
    background-color: #eff3ff;
  }

  & .copy {
    margin-left: 10px;
  }
}
</style>

<template>
  <div class="link-saving-account flex-col">
    <div class="main">
      <content-wrapper v-if="!hasWitnessKey" heading="link_saving_account.title" content="link_saving_account.information"> </content-wrapper>

      <content-wrapper v-else heading="link_saving_account.title">
        <div v-if="account.balance > 0">
          <div class="qr" @click="copyQr">
            <vue-qrcode ref="qrcode" class="qrcode" :width="280" :margin="0" :value="witnessKey" :color="{ dark: `#000000`, light: `#FFFFFF` }" />
          </div>
          <div class="address-row flex-row">
            <div class="flex-1" />
            <clipboard-field class="address" :value="witnessKey" confirmation="receive_coins.address_copied_to_clipboard"></clipboard-field>
            <div class="flex-1" />
          </div>
        </div>
        <div v-if="account.balance === 0">
          <p class="information">{{ $t("link_saving_account.no_funds") }}</p>
        </div>
      </content-wrapper>
    </div>
    <app-button-section>
      <button @click="tryGetWitnessKey" v-if="!hasWitnessKey">
        {{ $t("buttons.next") }}
      </button>
    </app-button-section>
  </div>
</template>
<script>
import { mapGetters } from "vuex";
import { clipboard, nativeImage } from "electron";
import { AccountsController } from "../../../unity/Controllers";
import VueQrcode from "vue-qrcode";
import EventBus from "../../../EventBus";

export default {
  name: "LinkSavingAccount",
  components: {
    VueQrcode
  },
  data() {
    return {
      witnessKey: null
    };
  },
  computed: {
    ...mapGetters("wallet", ["account"]),
    hasWitnessKey() {
      return this.witnessKey !== null;
    }
  },
  methods: {
    tryGetWitnessKey() {
      EventBus.$emit("unlock-wallet", {
        callback: async () => {
          this.witnessKey = AccountsController.GetWitnessKeyURI(this.account.UUID);
        }
      });
    },
    copyQr() {
      let img = nativeImage.createFromDataURL(this.$refs.qrcode.$el.src);
      clipboard.writeImage(img);
    }
  }
};
</script>

<style lang="less" scoped>
.link-saving-account {
  height: 100%;
  text-align: center;

  .main {
    flex: 1;
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
    display: flex;
    flex-direction: row;
    justify-content: center;
    flex-wrap: wrap;
  }
  & .address {
    width: 100%;
    margin: 5px 0 0 0;
    font-weight: 500;
    font-size: 1em;
    line-height: 1.4em;
    word-wrap: break-word;
  }
}
</style>

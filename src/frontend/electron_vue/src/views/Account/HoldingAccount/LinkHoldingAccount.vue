<template>
  <div class="link-witness-view flex-col">
    <portal to="header-slot">
      <main-header :title="$t(`link_holding_account.title`)"></main-header>
    </portal>
    <div class="main">
      <h4>{{ $t("link_holding_account.title") }}</h4>
      <p class="information">{{ $t("link_holding_account.information") }}</p>
      <div>
        <app-form-field
          style="text-align: left;"
          :title="$t(`common.password`)"
          v-if="walletPassword === null || walletPassword === undefined"
        >
          <input
            v-model="password"
            type="password"
            :class="passwordClass"
            @keydown="onPasswordKeydown"
          />
        </app-form-field>
      </div>
      <div>
        <div v-if="account.balance > 0 && walletPassword">
          <div class="qr" @click="copyQr">
            <vue-qrcode
              ref="qrcode"
              class="qrcode"
              :width="280"
              :margin="0"
              :value="witnessKey"
              :color="{ dark: `#000000`, light: `#FFFFFF` }"
            />
          </div>
          <div class="address-row flex-row">
            <div class="flex-1" />
            <clipboard-field
              class="address"
              :value="witnessKey"
              confirmation="receive_coins.address_copied_to_clipboard"
            ></clipboard-field>
            <div class="flex-1" />
          </div>
        </div>
        <div v-if="account.balance === 0 && walletPassword">
          <p class="information">{{ $t("link_holding_account.no_funds") }}</p>
        </div>
      </div>
    </div>
    <app-button-section>
      <button
        @click="unLockAccount"
        v-if="walletPassword === null || walletPassword === undefined"
      >
        {{ $t("buttons.next") }}
      </button>
    </app-button-section>
  </div>
</template>
<script>
import { mapState, mapGetters } from "vuex";
import { clipboard, nativeImage } from "electron";
import {
  LibraryController,
  AccountsController
} from "../../../unity/Controllers";
import VueQrcode from "vue-qrcode";
export default {
  name: "LinkHoldingAccount",
  components: {
    VueQrcode
  },
  data() {
    return {
      witnessKey: "",
      isPasswordInvalid: false,
      password: ""
    };
  },
  computed: {
    ...mapState("wallet", ["walletPassword"]),
    ...mapGetters("wallet", ["account"]),
    computedPassword() {
      return this.walletPassword ? this.walletPassword : this.password || "";
    },
    passwordClass() {
      return this.isPasswordInvalid ? "error" : "";
    },
    hasErrors() {
      return this.isPasswordInvalid;
    }
  },
  mounted() {
    if (this.walletPassword) {
      this.getWitnessKey(this.walletPassword);
    }
  },
  methods: {
    getWitnessKey() {
      this.witnessKey = AccountsController.GetWitnessKeyURI(this.account.UUID);
    },
    copyQr() {
      let img = nativeImage.createFromDataURL(this.$refs.qrcode.$el.src);
      clipboard.writeImage(img);
    },
    onPasswordKeydown() {
      this.isPasswordInvalid = false;
    },
    unLockAccount() {
      if (LibraryController.UnlockWallet(this.password)) {
        this.getWitnessKey();
        LibraryController.LockWallet();
        this.$store.dispatch("wallet/SET_WALLET_PASSWORD", this.password);
      } else {
        this.isPasswordInvalid = true;
      }
    }
  }
};
</script>

<style lang="less" scoped>
.link-witness-view {
  height: 100%;
  text-align: center;

  .main {
    flex: 1;
  }

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

input {
  background-color: #eee;
}
</style>

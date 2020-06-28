<template>
  <div class="send-view">
    <div class="back">
      <router-link :to="{ name: 'wallet' }">
        <fa-icon :icon="['fal', 'long-arrow-left']" />
        <span> {{ $t("buttons.back") }}</span>
      </router-link>
    </div>

    <h2>{{ $t("wallet.send_novo") }}</h2>

    <div class="row">
      <h4>{{ $t("common.receiving_address") }}:</h4>
      <input v-model="address" :class="addressClass" @input="onAddressInput" />
    </div>
    <div class="row">
      <h4>{{ $t("common.amount") }}:</h4>
      <currency-input v-model="amount" currency="N" />
    </div>

    <div class="row">
      <h4>{{ $t("common.password") }}:</h4>
      <input
        ref="password"
        type="password"
        v-model="password"
        :class="passwordClass"
        @input="onPasswordInput"
      />
    </div>

    <div class="button-wrapper">
      <button class="btn" @click="sendCoins">{{ $t("buttons.send") }}</button>
    </div>
  </div>
</template>

<style scoped lang="less">
.back a {
  padding: 4px 8px;
  margin: 0 0 0 -8px;
}
.back {
  margin-bottom: 20px;
}

.back a:hover {
  background-color: #f5f5f5;
}

.row {
  margin: 0 0 20px 0;
}

.arrow {
  float: right;
  margin: 6px 0 0 0;
  color: #fff;
}

.button-wrapper {
  float: right;
}

a:hover > .arrow {
  color: #000;
}
</style>

<script>
import UnityBackend from "../unity/UnityBackend";
export default {
  data() {
    return {
      address: "",
      amount: null,
      password: "",
      isAddressInvalid: false,
      isPasswordInvalid: false
    };
  },
  computed: {
    addressClass() {
      return this.isAddressInvalid ? "error" : "";
    },
    passwordClass() {
      return this.isPasswordInvalid ? "error" : "";
    }
  },
  methods: {
    sendCoins() {
      var check = UnityBackend.IsValidRecipient({
        scheme: "gulden",
        path: this.address,
        items: []
      });
      if (check.valid === false) {
        this.isAddressInvalid = true;
        return;
      }

      if (this.password.length === 0) {
        this.isPasswordInvalid = true;
        return;
      }
      if (!UnityBackend.UnlockWallet(this.password)) {
        this.isPasswordInvalid = true;
        return;
      }

      var request = {
        valid: "true",
        address: this.address,
        label: "",
        desc: "",
        amount: this.amount * 100000000.0
      };
      var ret = UnityBackend.PerformPaymentToRecipient(request, false);
      if (ret !== 0) {
        alert("failed to make payment");
        UnityBackend.LockWallet();
        return;
      } else {
        this.address = "";
      }
      UnityBackend.LockWallet();
    },
    onAddressInput() {
      this.isAddressInvalid = false;
    },
    onPasswordInput() {
      this.isPasswordInvalid = false;
    }
  }
};
</script>

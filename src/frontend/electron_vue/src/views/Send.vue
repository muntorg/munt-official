<template>
  <div class="send-view">
    <novo-section class="back">
      <router-link :to="{ name: 'wallet' }">
        <fa-icon :icon="['fal', 'long-arrow-left']" />
        <span> {{ $t("buttons.back") }}</span>
      </router-link>
    </novo-section>

    <h2>{{ $t("wallet.send_novo") }}</h2>

    <novo-form-field :title="$t('common.receiving_address')">
      <input v-model="address" :class="addressClass" @input="onAddressInput" />
    </novo-form-field>
    <novo-form-field :title="$t('common.amount')">
      <currency-input v-model="amount" currency="N" />
    </novo-form-field>

    <novo-form-field :title="$t('common.password')">
      <input
        ref="password"
        type="password"
        v-model="password"
        :class="passwordClass"
        @input="onPasswordInput"
      />
    </novo-form-field>

    <novo-button-section>
      <button @click="sendCoins">{{ $t("buttons.send") }}</button>
    </novo-button-section>
  </div>
</template>

<style scoped lang="less">
.back a {
  padding: 4px 8px;
  margin: 0 0 0 -8px;

  &:hover {
    background-color: #f5f5f5;
  }
}

.arrow {
  float: right;
  margin: 6px 0 0 0;
  color: #fff;
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

<template>
  <div class="settings">
    <div class="section">
      <div class="back">
        <router-link :to="{ name: 'wallet' }">
          <fa-icon :icon="['fal', 'long-arrow-left']" />
          <span> {{ $t("buttons.back") }}</span>
        </router-link>
      </div>

      <h2>{{ "Send coins" }}</h2>
      <div class="password">
        <div class="password-row">
          <h4>{{ "address" }}:</h4>
          <novo-input v-model="address" />
        </div>
        <div class="password-row">
          <h4>{{ "amount" }}:</h4>
          <currency-input currency="N" v-model="amount" />
        </div>
      </div>

      <div class="password">
        <div class="password-row">
          <h4>{{ "Password" }}:</h4>
          <novo-input ref="password" type="password" v-model="password" />
        </div>
      </div>

      <div class="button-wrapper">
        <novo-button class="btn" @click="sendCoins"
          >{{ "Send coins" }}
        </novo-button>
      </div>
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

.settings-row {
  padding: 4px 0;
  border-bottom: 1px solid #ccc;
}

.settings-row a {
  display: inline-block;
  width: 100%;
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
      amount: 0,
      pasword: ""
    };
  },
  methods: {
    sendCoins() {
      var check = UnityBackend.IsValidRecipient({
        scheme: "gulden",
        path: this.address,
        items: []
      });
      if (check.valid === false) {
        alert("invalid address");
        return;
      }

      if (this.password.length === 0) {
        alert("invalid password");
        return;
      }
      if (!UnityBackend.UnlockWallet(this.password)) {
        alert("invalid password");
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
      if (ret != 0) {
        alert("failed to make payment");
        UnityBackend.LockWallet();
        return;
      } else {
        this.address = "";
      }
      UnityBackend.LockWallet();
    }
  }
};
</script>

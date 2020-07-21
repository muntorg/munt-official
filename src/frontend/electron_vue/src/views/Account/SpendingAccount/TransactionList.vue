<template>
  <div class="transaction-list">
    <div class="transactions-header">
      <span class="resend-transaction"></span>
      <span class="transactions-address">recipient</span>
      <span class="transactions-amount">amount</span>
      <span class="transactions-hash">hash</span>
      <span class="transactions-depth">depth</span>
      <span class="transactions-status">status</span>
    </div>
    <div
      class="transaction"
      v-for="row in transactions"
      v-bind:key="row.txHash"
    >
      <span class="resend-transaction"
        ><button @click="resubmitTransaction(row.txHash)">
          resend
        </button></span
      >
      <span class="transactions-address">{{
        getRecipients(row.inputs, row.outputs)
      }}</span>
      <span class="transactions-amount">{{ row.amount / 100000000 }}</span>
      <span class="transactions-hash">{{ row.txHash }}</span>
      <span class="transactions-depth">{{ row.depth }}</span>
      <span class="transactions-status">{{ row.status }}</span>
    </div>
  </div>
</template>

<script>
import UnityBackend from "../../../unity/UnityBackend";

export default {
  name: "TransactionList",
  data() {
    return {
      transactions: UnityBackend.GetTransactionHistory()
    };
  },
  methods: {
    resubmitTransaction(txHash) {
      UnityBackend.ResendTransaction(txHash);
      //console.log("Resent transaction: ["+rawTx+"]")
    },
    getRecipients(inputs, outputs) {
      let ret = "";
      let i;
      let didISend = false;
      for (i = 0; i < inputs.length; i++) {
        if (inputs[i].isMine === true) {
          didISend = true;
        }
      }
      if (didISend === false) {
        return "n/a";
      }

      for (i = 0; i < outputs.length; i++) {
        if (outputs[i].isMine === false) {
          if (ret === "") ret = ret + outputs[i].address;
          else ret = ret + ", " + outputs[i].address;
        }
      }
      return ret;
    }
  }
};
</script>

<style scoped lang="less">
.transaction-list {
  float: left;
  width: 100%;
  overflow: scroll-y;
}

.transactions-header {
  float: left;
  width: 100%;
  font-size: 0.8em;
  font-weight: 500;
}

.transaction {
  float: left;
  width: 100%;
  padding: 10px 0 10px 0;
  user-select: text;
  font-size: 0.8em;
  line-height: 1.2em;
}

.transactions-address {
  float: left;
  width: 45%;
  user-select: text;
  overflow-wrap: break-word;
}

.transactions-amount {
  float: left;
  width: 15%;
  user-select: text;
}

.transactions-hash {
  float: left;
  width: 20%;
  overflow-wrap: break-word;
  user-select: text;
}

.transactions-depth {
  float: left;
  width: 10%;
  text-align: right;
}

.transactions-status {
  float: left;
  width: 10%;
  text-align: right;
}

.selectall {
  user-select: all;
}
</style>

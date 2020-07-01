<template>
  <div class="send-view">
    <novo-section class="back">
      <router-link :to="{ name: 'wallet' }">
        <fa-icon :icon="['fal', 'long-arrow-left']" />
        <span> {{ $t("buttons.back") }}</span>
      </router-link>
    </novo-section>

    <table id="transactionTable">
      <thead>
        <tr>
          <th>recipient</th>
          <th>amount</th>
          <th>hash</th>
          <th>depth</th>
          <th>status</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="row in transactions" v-bind:key="row.txHash">
          <td>{{ getRecipients(row.inputs, row.outputs) }}</td>
          <td>{{ row.amount / 100000000 }}</td>
          <td>{{ row.txHash }}</td>
          <td>{{ row.depth }}</td>
          <td>{{ row.status }}</td>
        </tr>
      </tbody>
    </table>
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
      transactions: UnityBackend.GetTransactionHistory()
    };
  },
  computed: {},
  methods: {
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

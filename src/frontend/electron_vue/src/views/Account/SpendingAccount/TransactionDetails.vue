<template>
  <div class="transaction-details flex-col">
    <portal to="sidebar-right-title">
      {{ $t("transaction_details.title") }}
    </portal>

    <div class="main">
      <h5>{{ $t("transaction_details.hash") }}</h5>
      <div class="tx-hash">{{ txHash }}</div>
    </div>
  </div>
</template>

<script>
export default {
  name: "TransactionDetails",
  props: {
    txHash: null
  }
};
</script>

<style lang="less" scoped>
.transaction-details {
  height: 100%;

  .main {
    flex: 1;
  }

  .tx-address {
    margin: 0 0 20px 0;
    padding: 10px;
    font-size: 0.9em;
    line-height: 1.2em;
    user-select: text;
    word-wrap: break-word;
    background-color: #fff;
  }

  .tx-hash {
    margin: 0;
    padding: 10px;
    font-size: 0.8em;
    line-height: 1.2em;
    user-select: text;
    word-wrap: break-word;
    background-color: #fff;
  }
}
</style>

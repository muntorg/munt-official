<template>
  <div class="transaction-details flex-col">
    <portal to="sidebar-right-title">
      Transaction details
    </portal>

    <div class="main">
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

  .tx-hash {
    font-size: 0.55rem;
  }
}
</style>

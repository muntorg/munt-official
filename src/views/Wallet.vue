<template>
  <div id="wallet">
    <div class="section">
      <h4>{{ $t("balance.your_address") }}</h4>
      <span id="address">{{ receiveAddress }}</span>
    </div>
    <div class="transfer"><a target="_blank" href="https://novocurrency.com/transfer?receive_address={{ receiveAddress }}" id="transfer">Receive your acquired Novo</a></div>
  </div>
</template>

<script>
import { mapState } from "vuex";

export default {
  computed: {
    ...mapState(["receiveAddress"])
  }
};
</script>

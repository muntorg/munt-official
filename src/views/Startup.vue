<template>
  <div></div>
</template>

<script>
import { mapState } from "vuex";
import { AppStatus } from "../store";

export default {
  name: "Startup",
  computed: {
    ...mapState(["status"])
  },
  mounted() {
    console.log("startup mounted");
    this.onStatusChanged();
  },
  watch: {
    status() {
      console.log("watch:status");
      this.onStatusChanged();
    }
  },
  methods: {
    onStatusChanged() {
      console.log(`startup:onStatusChanged -> ${this.status}`);
      switch (this.status) {
        case AppStatus.setup:
          this.routeTo("setup");
          break;
        case AppStatus.synchronize:
        case AppStatus.ready:
          this.routeTo("wallet");
          break;
      }
    },
    routeTo(name) {
      if (this.$router.name === name) return;
      console.log(`routeTo -> ${name}`);
      this.$router.push({ name: name });
    },
    setStatus(status) {
      this.$store.dispatch({ type: "SET_STATUS", status });
    }
  }
};
</script>

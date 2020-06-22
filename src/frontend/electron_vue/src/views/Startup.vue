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
    this.routeOnStatusChange();
  },
  watch: {
    status() {
      this.routeOnStatusChange();
    }
  },
  methods: {
    routeOnStatusChange() {
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
      this.$router.push({ name: name });
    }
  }
};
</script>

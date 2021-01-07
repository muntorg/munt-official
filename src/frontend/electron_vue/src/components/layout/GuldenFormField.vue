<template>
  <gulden-section class="gulden-form-field">
    <h4 v-if="hasTitle">{{ title }}</h4>
    <div class="input">
      <slot />
    </div>
  </gulden-section>
</template>

<script>
import GuldenSection from "./GuldenSection";

export default {
  name: "GuldenFormField",
  props: {
    title: {
      type: String,
      default: ""
    }
  },
  components: {
    GuldenSection
  },
  computed: {
    hasTitle() {
      return this.title !== null && this.title.trim().length > 0;
    }
  }
};
</script>

<style lang="less" scoped>
h4 {
  margin-bottom: 10px;
}
</style>

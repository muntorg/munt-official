<template>
  <app-section class="app-form-field">
    <h4 v-if="hasTitle">{{ title }}</h4>
    <slot></slot>
  </app-section>
</template>

<script>
import AppSection from "./AppSection";

export default {
  name: "AppFormField",
  props: {
    title: {
      type: String,
      default: ""
    }
  },
  components: {
    AppSection
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

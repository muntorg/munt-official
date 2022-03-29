<template>
  <app-section class="app-form-field">
    <h2 v-if="hasTitle">{{ $t(title) }}</h2>
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

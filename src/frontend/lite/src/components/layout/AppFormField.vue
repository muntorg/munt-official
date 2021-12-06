<template>
  <app-section class="app-form-field">
    <h3 v-if="hasTitle">{{ title }}</h3>
    <div class="input">
      <slot />
    </div>
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

<template>
  <novo-section>
    <h4 v-if="hasTitle">{{ title }}:</h4>
    <div class="input">
      <slot />
    </div>
  </novo-section>
</template>

<script>
import NovoSection from "./NovoSection";

export default {
  name: "NovoFormField",
  props: {
    title: {
      type: String,
      default: ""
    }
  },
  components: {
    NovoSection
  },
  computed: {
    hasTitle() {
      return this.title !== null && this.title.trim().length > 0;
    }
  }
};
</script>

<style lang="less" scoped>
.input {
  background: hotpink;
}
</style>

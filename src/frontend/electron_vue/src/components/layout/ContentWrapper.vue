<template>
  <div>
    <h2 v-if="heading" :class="headingStyle">{{ $t(heading) }}</h2>
    <p v-if="content">
      {{ $t(content) }}
    </p>
    <slot></slot>
  </div>
</template>

<script>
export default {
  name: "ContentWrapper",
  props: {
    heading: {
      type: String,
      default: null
    },
    headingStyle: {
      type: String,
      default: null,
      validator: value => {
        return ["warning"].includes(value);
      }
    },
    content: {
      type: String,
      default: null
    }
  }
};
</script>

<style scoped>
p {
  text-align: inherit;
  white-space: pre-line; /* Allow \n to be treated as newline */
}

h2.warning {
  color: var(--error-color);
}
</style>

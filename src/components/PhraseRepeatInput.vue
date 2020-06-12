<template>
  <input
    type="text"
    @keyup="onKeyUp"
    v-bind:class="{ match: match, error: match === false }"
  />
</template>

<script>
export default {
  name: "PhraseRepeatInput",
  data() {
    return {
      match: null
    };
  },
  props: {
    word: String
  },
  methods: {
    onKeyUp() {
      console.log(`${this.$el.value} : ${this.word}`);

      let match = null;
      if (this.$el.value === this.word) {
        match = true;
      } else if (this.$el.value.length > this.word.length) {
        match = false;
      } else {
        match = null;
        for (var i = 0; i < this.$el.value.length; i++) {
          if (this.$el.value.charAt(i) !== this.word.charAt(i)) {
            match = false;
            break;
          }
        }
      }
      if (this.match === match) return;

      if (match) {
        this.$emit("match-changed", true);
      } else if (this.match) {
        this.$emit("match-changed", false);
      }

      this.match = match;
    }
  }
};
</script>

<style lang="less">
input {
  color: var(--success-color, #009572);
  border: 1px solid #ddd;
  background-color: #fff;
}

input:focus {
  border: 1px solid var(--success-color, #009572);
}

input.match,
input.match:focus {
  color: var(--success-text-color, #fff);
  border-color: var(--success-color, #009572);
  background: var(--success-color, #009572);
}

input.error,
input:focus.error {
  color: var(--error-text-color, #fff);
  border-color: var(--error-color, red);
  background: var(--error-color, red);
}
</style>

<template>
  <app-input
    class="phrase-repeat-input"
    :status="status"
    type="text"
    v-model="input"
    @focus="onFocus"
  />
</template>

<script>
import AppInput from "./AppInput";

export default {
  name: "PhraseRepeatInput",
  data() {
    return {
      input: "",
      match: null
    };
  },
  props: {
    word: String,
    tabOnMatch: {
      type: Boolean,
      default: true
    }
  },
  components: {
    AppInput
  },
  computed: {
    status() {
      return this.match ? "success" : this.match === false ? "error" : "";
    }
  },
  watch: {
    input() {
      this.validate(this.input);
    }
  },
  methods: {
    onFocus() {
      console.log(this.word);
    },
    validate(value) {
      let match = null;

      if (value === this.word) {
        match = true;
      } else if (value.length > this.word.length) {
        match = false;
      } else {
        match = null;
        for (var i = 0; i < value.length; i++) {
          if (value.charAt(i) !== this.word.charAt(i)) {
            match = false;
            break;
          }
        }
      }
      if (this.match === match) return;

      if (match) {
        this.$emit("match-changed", true);
        if (this.tabOnMatch) {
          let nextEl = event.srcElement.nextSibling;
          if (nextEl && nextEl.tagName === "INPUT") {
            nextEl.focus();
          }
        }
      } else if (this.match) {
        this.$emit("match-changed", false);
      }

      this.match = match;
    }
  }
};
</script>

<style lang="less" scoped>
.phrase-repeat-input {
  &.app-input.success,
  &.app-input.success:focus {
    border-color: #ccc;
  }
}
</style>

<template>
  <div class="phrase-validator-wrapper">
    <app-input
      v-for="(word, index) in words"
      :key="index"
      class="repeater-input"
      ref="repeater"
      @input="validateWord(index, $event)"
      @focus="onFocus(index)"
      v-model="repeaterWords[index]"
      :status="getStatus(index)"
      :readonly="isValid(index)"
    />
  </div>
</template>

<script>
import AppInput from "./AppInput";

export default {
  name: "PhraseValidator",
  props: {
    phrase: {
      type: String,
      default: ""
    },
    autofocus: {
      type: Boolean,
      default: true
    }
  },
  data() {
    return {
      repeaterWords: []
    };
  },
  mounted() {
    if (this.autofocus) {
      this.$nextTick(() => {
        this.$refs.repeater[0].$el.focus();
      });
    }
  },
  components: {
    AppInput
  },
  computed: {
    words() {
      return this.phrase.split(" ");
    },
    phraseRepeat() {
      return this.repeaterWords.join(" ");
    }
  },
  methods: {
    onFocus(index) {
      console.log(this.words[index]);
    },
    isValid(index) {
      let repeaterWord = this.repeaterWords[index];
      let word = this.words[index];
      return word === repeaterWord;
    },
    validateWord(index, e) {
      let word = this.words[index];
      if (word === e) {
        let next = this.$refs.repeater[index + 1];
        if (next) next.$el.focus();
        if (this.phrase === this.phraseRepeat)
          this.$emit("validated", this.phraseRepeat);
      }
    },
    getStatus(index) {
      let repeaterWord = this.repeaterWords[index];
      if (repeaterWord === undefined) return "";

      let word = this.words[index];
      if (word === repeaterWord) {
        return "success";
      } else if (repeaterWord.length > word.length) {
        return "error";
      } else {
        return word.indexOf(repeaterWord) === 0 ? "" : "error";
      }
    }
  }
};
</script>

<style lang="less" scoped>
.repeater-input {
  width: calc(25% - 10px);
  margin: 5px;
  text-align: center;

  &.success,
  &.success:focus {
    border-color: #ccc;
  }
}
</style>

<template>
  <div class="phrase-input-wrapper">
    <input
      v-for="(word, index) in words"
      :key="index"
      type="text"
      class="word-input"
      ref="wordinput"
      @input="onInput(index, $event)"
      @focus="onFocus(index)"
      v-model="inputWords[index]"
      :class="getCssClass(index)"
      :disabled="isExactMatch(index)"
    />
  </div>
</template>

<script>
export default {
  name: "PhraseInput",
  props: {
    validate: {
      type: [String, Object],
      default: ""
    },
    isPhraseInvalid: {
      type: Boolean,
      default: false
    },
    autofocus: {
      type: Boolean,
      default: true
    }
  },
  data() {
    return {
      inputWords: []
    };
  },
  mounted() {
    if (this.autofocus) {
      this.$nextTick(() => {
        this.$refs.wordinput[0].focus();
      });
    }
  },
  computed: {
    isRecovery() {
      return typeof this.validate !== "string";
    },
    words() {
      return this.isRecovery
        ? Array(this.validate.length)
            .join(".")
            .split(".")
        : this.validate.split(" ");
    },
    isPhraseComplete() {
      let phraseComplete = true;
      for (var i = 0; i < this.words.length; i++) {
        if (this.isMatch(i) == false) {
          phraseComplete = false;
          break;
        }
      }
      return phraseComplete;
    }
  },
  methods: {
    clear() {
      this.inputWords = [];
      this.$nextTick(() => {
        this.$refs.wordinput[0].focus();
      });
    },
    onFocus(index) {
      let newIndex = 0;
      for (var i = 0; i <= index; i++) {
        if (this.isMatch(i) === false) {
          newIndex = i;
          break;
        }
      }
      if (index !== newIndex) {
        this.$refs.wordinput[newIndex].focus();
        return;
      }

      let word = this.words[index];
      if (word !== "") console.log(word);
    },
    onInput(index) {
      if (this.isExactMatch(index) === false) return;

      if (this.isPhraseComplete) {
        this.$emit("phrase-complete", this.inputWords.join(" "));
      } else {
        let next = this.$refs.wordinput[index + 1];
        if (next) next.focus();
      }
    },
    getCssClass(index) {
      if (this.isPhraseInvalid) return "";
      if (this.isMatch(index)) return "success";
      return this.isInvalid(index) ? "error" : "";
    },
    isExactMatch(index) {
      let inputWord = this.inputWords[index];
      if (inputWord === undefined || inputWord.length === 0) return false;

      if (this.isRecovery) {
        let validWords = this.getValidWords(index);
        return validWords.length === 1 && inputWord === validWords[0];
      } else {
        return inputWord === this.words[index];
      }
    },
    isMatch(index) {
      if (this.isRecovery) {
        let inputWord = this.inputWords[index];
        if (inputWord === undefined || inputWord.length === 0) return false;
        let validWords = this.getValidWords(index);
        return validWords.indexOf(inputWord) !== -1;
      } else return this.isExactMatch(index);
    },
    isInvalid(index) {
      let inputWord = this.inputWords[index];
      if (inputWord === undefined || inputWord.length === 0) return false;

      if (this.isRecovery) {
        return this.getValidWords(index).length === 0;
      } else {
        let word = this.words[index];

        return inputWord.length > word.length
          ? true
          : word.indexOf(inputWord) === -1;
      }
    },
    getValidWords(index) {
      return this.validate.words.filter(
        x => x.indexOf(this.inputWords[index]) === 0
      );
    }
  }
};
</script>

<style lang="less" scoped>
.phrase-input-wrapper {
  width: calc(100% + 10px);
  margin: 0 0 0 -5px;
}

.word-input {
  width: calc(25% - 10px);
  margin: 5px;

  &.success,
  &.success:focus {
    border-color: #ccc;
  }
}
</style>

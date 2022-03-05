<template>
  <div class="phrase-input-wrapper">
    <input
      v-for="(word, index) in words"
      :key="index"
      type="text"
      class="word-input"
      ref="input"
      @input="onInput(index, $event)"
      @focus="onFocus(index)"
      @blur="onBlur(index)"
      @keydown="validatePraseOnEnter"
      v-model="inputs[index]"
      :class="getCssClass(index)"
      :disabled="isInputDisabled(index)"
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
      inputs: []
    };
  },
  mounted() {
    if (this.autofocus) {
      this.$nextTick(() => {
        this.$refs.input[0].focus();
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
    }
  },
  methods: {
    clear() {
      this.inputs = [];
      this.$nextTick(() => {
        this.$refs.input[0].focus();
      });
    },
    onFocus(index) {
      // force sequential input. make sure the first input is focused which doesn't have a valid word
      for (var i = 0; i < index; i++) {
        if (this.isMatch(i) === false) {
          // this input doesn't contain a valid word, so put the focus on that word
          this.$refs.input[i].focus();
          return;
        }
      }

      if (this.isMatch(index)) {
        // focus set on input by the user, so let's clear the input
        this.inputs[index] = "";
        this.$refs.input[index].value = "";
        this.$emit("possible-phrase", null);
      }
      if (!this.isRecovery) console.log(this.words[index]);
    },
    onBlur(index) {
      if (this.isMatch(index) === false) {
        this.inputs[index] = "";
        this.$refs.input[index].value = "";
      }
    },
    onInput(index) {
      if (this.inputs.length === 12 && this.isMatch(index)) {
        this.$emit("possible-phrase", this.inputs.join(" "));
        return;
      }
      this.$emit("possible-phrase", null);

      if (this.isExactMatch(index)) {
        let nextIndex;
        for (
          nextIndex = index + 1;
          nextIndex <= this.inputs.length;
          nextIndex++
        ) {
          if (this.inputs[nextIndex] === undefined) break;
          if (this.isMatch(nextIndex) === false) break;
        }

        let next = this.$refs.input[nextIndex];
        if (next) next.focus();
      }
    },
    getCssClass(index) {
      if (this.isPhraseInvalid) return "";
      if (this.isInvalid(index)) return "error";
      if (this.isMatch(index)) return "success";
      return "";
    },
    isInputDisabled(index) {
      if (this.isRecovery) return false;
      if (index === this.words.length - 1) return false;
      return this.isExactMatch(index);
    },
    isExactMatch(index) {
      let inputWord = this.inputs[index];
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
        let inputWord = this.inputs[index];
        if (inputWord === undefined || inputWord.length === 0) return false;
        return this.getValidWords(index).indexOf(inputWord) !== -1;
      } else return this.isExactMatch(index);
    },
    isInvalid(index) {
      let inputWord = this.inputs[index];
      if (inputWord === undefined || inputWord.length === 0) return false;

      if (this.isRecovery) {
        return this.getValidWords(index).length === 0;
      } else {
        let word = this.words[index];
        return inputWord.length > word.length
          ? true
          : word.indexOf(inputWord) !== 0;
      }
    },
    getValidWords(index) {
      return this.validate.words.filter(
        x => x.indexOf(this.inputs[index]) === 0
      );
    },
    validatePraseOnEnter(event) {
      if (event.keyCode === 13) this.$emit("enter");
    }
  }
};
</script>

<style lang="less" scoped>
.phrase-input-wrapper {
  width: calc(100% + 10px);
  margin: -5px 0 0 -5px;
}

.word-input {
  width: calc(25% - 10px);
  margin: 5px;
}
</style>

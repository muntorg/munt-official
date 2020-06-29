<template>
  <div class="phrase-validator-wrapper">
    <input
      v-for="(word, index) in words"
      :key="index"
      class="repeater-input"
      ref="repeater"
      @input="validateWord(index, $event)"
      @focus="onFocus(index)"
      v-model="repeaterWords[index]"
      :class="getStatus(index)"
      :readonly="isValid(index)"
    />
  </div>
</template>

<script>
import UnityBackend from "../unity/UnityBackend";
export default {
  name: "PhraseValidator",
  props: {
    phrase: {
      type: String,
      default: ""
    },
    isRecovery: {
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
      repeaterWords: []
    };
  },
  mounted() {
    if (this.autofocus) {
      this.$nextTick(() => {
        this.$refs.repeater[0].focus();
      });
    }
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
      if (this.isRecovery) {
        if (UnityBackend.IsValidRecoveryPhrase(this.phraseRepeat)) {
          this.$emit("validated", this.phraseRepeat);
        }
      }
      let word = this.words[index];
      if (word === e.target.value) {
        let next = this.$refs.repeater[index + 1];
        if (next) next.focus();
        if (this.phrase === this.phraseRepeat)
          this.$emit("validated", this.phraseRepeat);
      }
    },
    getStatus(index) {
      if (this.isRecovery) return "success";

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
.phrase-validator-wrapper {
  width: calc(100% + 10px);
  margin: 0 0 0 -5px;
}

.repeater-input {
  width: calc(25% - 10px);
  margin: 5px;

  &.success,
  &.success:focus {
    border-color: #ccc;
  }
}
</style>

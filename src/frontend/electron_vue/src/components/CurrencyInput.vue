<template>
  <input
    type="text"
    v-model="displayValue"
    @focus="isFocussed = true"
    @blur="isFocussed = false"
    @keydown="validateInput"
    @paste="preventPaste"
  />
</template>

<script>
export default {
  name: "CurrencyInput",
  props: {
    value: {
      type: [String, Number],
      default: ""
    },
    currency: {
      type: String,
      default: ""
    }
  },
  data() {
    return {
      isFocussed: false,
      innerValue: this.value
    };
  },
  watch: {
    innerValue() {
      this.$emit("input", this.innerValue);
    }
  },
  computed: {
    displayValue: {
      get: function() {
        if (
          this.innerValue === null ||
          this.innerValue === undefined ||
          this.innerValue.toString().trim() === ""
        )
          return null;

        if (this.isFocussed) {
          return this.innerValue;
        } else {
          return `${this.currency} ${this.innerValue
            .toString()
            .replace(/(\d)(?=(\d{3})+(?:\.\d+)?$)/g, "$1 ")}`.trim();
        }
      },
      set: function(modifiedValue) {
        let newValue = modifiedValue
          .replace(/[^0-9.]/g, "")
          .replace(/(\..*)\./g, "$1");

        let periodIdx = newValue.indexOf(".");
        if (periodIdx !== -1 && newValue.length - periodIdx > 3) {
          newValue = `${newValue.slice(0, periodIdx)}${newValue.slice(
            periodIdx + 1,
            periodIdx + 2
          )}.${newValue.slice(periodIdx + 2)}`;
        }

        this.innerValue = newValue;
      }
    }
  },
  methods: {
    preventPaste(e) {
      e.preventDefault();
    },
    validateInput(e) {
      if (
        (e.keyCode >= 48 && e.keyCode <= 58 && e.shiftKey == false) || // 0-9
        (e.keyCode >= 96 && e.keyCode <= 105) || // 0-9 numeric pad
        ((e.keyCode == 190 || e.keyCode === 110) &&
          this.displayValue.toString().indexOf(".") === -1) || // only allow single .
        e.keyCode === 8 || // backspace
        e.keyCode === 9 || // tab
        e.keyCode === 13 || // enter
        (e.keyCode >= 33 && e.keyCode <= 40) || // ...
        e.keyCode === 45 || // insert
        e.keyCode === 46 // delete
      )
        return;

      e.preventDefault();
    }
  }
};
</script>

<style scoped>
input {
  background-color: #eee;
}
</style>

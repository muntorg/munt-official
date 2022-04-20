<template>
  <div class="select-list-wrapper" :tabindex="tabindex" @blur="open = false">
    <div class="selected" :class="{ open: open }" @click="open = !open">
      {{ typeof selected === "object" ? selected.label : selected }}
    </div>
    <div class="arrow" @click="open = !open">
      <fa-icon :icon="['fal', 'chevron-down']" />
    </div>
    <div class="items" :class="{ selectHide: !open }">
      <div
        v-for="(option, i) of options"
        :key="i"
        @click="
          selected = option;
          open = false;
          $emit('input', option);
        "
      >
        {{ typeof option === "object" ? option.label : option }}
      </div>
    </div>
  </div>
</template>

<script>
export default {
  name: "SelectList",
  props: {
    options: {
      type: Array,
      required: true
    },
    default: {
      type: [String, Object],
      required: false,
      default: null
    },
    tabindex: {
      type: Number,
      required: false,
      default: 0
    }
  },
  data() {
    return {
      selected: this.default ? this.default : this.options.length > 0 ? this.options[0] : null,
      open: false
    };
  },
  mounted() {
    this.$emit("input", this.selected);
  }
};
</script>

<style lang="less" scoped>
.select-list-wrapper {
  position: relative;
  width: 100%;
  text-align: left;
  outline: none;
  line-height: 40px;
  font-style: normal;
  font-weight: 500;
  font-size: 14px;
  color: #000;
}

.select-list-wrapper .selected {
  padding-left: 10px;
  cursor: pointer;
  user-select: none;
  background-color: #fff;
  border: 1px solid #ccc;
}

.select-list-wrapper .selected.open {
  border: 1px solid #ccc;
}

.arrow {
  position: absolute;
  top: 0;
  right: 10px;
  font-size: 12px;
  line-height: 40px;
  cursor: pointer;
}

.select-list-wrapper .items {
  overflow: hidden;
  border-right: 1px solid #ccc;
  border-left: 1px solid #ccc;
  border-bottom: 1px solid #ccc;
  position: absolute;
  background-color: #fff;
  left: 0;
  right: 0;
  z-index: 1;
}

.select-list-wrapper .items div {
  padding-left: 1em;
  cursor: pointer;
  user-select: none;
}

.select-list-wrapper .items div:hover {
  color: #fff;
  background-color: var(--primary-color);
}

.selectHide {
  display: none;
}
</style>

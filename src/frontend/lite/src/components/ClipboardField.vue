<template>
  <div class="clipboard-field">
    <span class="field" @click="copyToClipboard">
      <span v-if="confirmCopy">
        {{ $t(confirmation) }}
      </span>
      <span v-else>
        {{ value }}
        <fa-icon :icon="['fal', 'copy']" class="copy" />
      </span>
    </span>
  </div>
</template>

<script>
import { clipboard } from "electron";

export default {
  name: "ClipboardField",
  props: {
    value: {
      type: String
    },
    confirmation: {
      type: String,
      default: "clipboard_field.copied_to_clipboard"
    }
  },
  data() {
    return {
      confirmCopy: false
    };
  },
  methods: {
    copyToClipboard() {
      clipboard.writeText(this.value);
      if (this.confirmation) {
        this.confirmCopy = true;
        setTimeout(() => {
          this.confirmCopy = false;
        }, 1500);
      }
    }
  }
};
</script>

<style lang="less" scoped>
.clipboard-field {
  text-align: center;
  padding: 10px 0;
}

.field {
  padding: 10px;
  cursor: pointer;
}

.field:hover {
  color: var(--primary-color);
  background-color: var(--hover-color);
}

.copy {
  margin-left: 10px;
}
</style>

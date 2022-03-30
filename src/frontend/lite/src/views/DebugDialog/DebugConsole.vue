<template>
  <div class="debug-console flex-col" v-if="show">
    <div class="output-buttons">
      <fa-icon
        :icon="['fal', 'search-minus']"
        class="button"
        @click="decreaseFontSize"
      />
      <fa-icon
        :icon="['fal', 'search-plus']"
        class="button"
        @click="increaseFontSize"
      />
      <fa-icon :icon="['fal', 'eraser']" class="button" @click="clearOutput" />
    </div>

    <div ref="output" class="output scrollable" :style="outputStyle">
      <div class="row">
        <div class="info">
          Use up and down arrows to navigate history. Type
          <span class="help">help</span> for an overview of available commands.
        </div>
      </div>
      <div class="row" v-for="(item, index) in output" :key="index">
        <fa-icon class="icon" :icon="getIcon(item.type)" />
        <pre class="data" :style="outputStyle">{{ item.data }}</pre>
      </div>
    </div>

    <div class="input">
      <input
        ref="command"
        type="text"
        spellcheck="false"
        v-model="command"
        @keydown="onRpcInputKeyDown"
        list="history"
      />
      <datalist id="history">
        <option
          v-for="item in filteredAutocompleteList"
          :key="item"
          :value="item"
        />
      </datalist>
    </div>
  </div>
</template>

<script>
import { RpcController } from "../../unity/Controllers";

export default {
  name: "DebugConsole",
  data() {
    return {
      command: "",
      fontSize: 0.8,
      history: [],
      output: [],
      index: 0,
      autocomplete: {
        all: [],
        command: null,
        filtered: [],
        disabled: false
      }
    };
  },
  props: {
    show: {
      type: Boolean
    }
  },
  computed: {
    outputStyle() {
      return `font-size: ${this.fontSize}rem;`;
    },
    filteredAutocompleteList() {
      return this.autocomplete.filtered.slice(0, 20);
    }
  },
  created() {
    this.autocomplete.all = RpcController.GetAutocompleteList();
  },
  watch: {
    output: {
      immediate: true,
      handler() {
        this.focusCommand();
      }
    },
    show: {
      immediate: true,
      handler() {
        if (this.show) {
          this.focusCommand();
        }
      }
    },
    command() {
      this.filterAutocompleteList();
    }
  },
  methods: {
    moveToEnd() {
      let command = this.$refs.command;
      setTimeout(function() {
        command.selectionStart = command.selectionEnd = command.value.length;
      }, 0);
    },
    getIcon(type) {
      return ["fal", `angle-double-${type === "command" ? "left" : "right"}`];
    },
    async onRpcInputKeyDown(e) {
      this.autocomplete.disabled = false;
      switch (e.keyCode) {
        case 13: {
          let result = await RpcController.ExecuteAsync(this.command);
          this.command = "";

          this.output.push({
            type: "command",
            data: result.command
          });
          this.output.push({ type: "result", ...result });

          this.scrollToBottom();

          let index = this.history.indexOf(result.command);
          if (index !== -1) {
            this.history.splice(index, 1);
          }
          this.history.push(result.command);
          this.index = this.history.length;
          return;
        }
        case 38: // arrow up
          if (this.index > 0) this.index--;
          if (this.history.length > 0) {
            this.autocomplete.disabled = true;
            this.command = this.history[this.index];
            this.moveToEnd();
            e.preventDefault();
          }
          break;
        case 40: // arrow down
          if (this.index < this.history.length) this.index++;
          if (this.index < this.history.length && this.history.length > 0) {
            this.autocomplete.disabled = true;
            this.command = this.history[this.index];
            this.moveToEnd();
            e.preventDefault();
          } else {
            this.command = "";
          }
          break;
      }
    },
    clearOutput() {
      this.output = [];
    },
    focusCommand() {
      this.$nextTick(() => {
        this.$refs.command.focus();
      });
    },
    scrollToBottom() {
      this.$nextTick(() => {
        this.$refs.output.scrollTop = this.$refs.output.scrollHeight;
      });
    },
    increaseFontSize() {
      if (this.fontSize >= 1.3) return;
      this.fontSize += 0.05;
    },
    decreaseFontSize() {
      if (this.fontSize <= 0.6) return;
      this.fontSize -= 0.05;
    },
    filterAutocompleteList() {
      if (this.autocomplete.disabled) return;

      let filteredList = [];
      const command =
        this.command && this.command.trim().length > 0
          ? this.command.trim()
          : null;
      if (command) {
        filteredList = command.startsWith(this.autocomplete.command)
          ? this.autocomplete.filtered
          : this.autocomplete.all;
      }

      this.autocomplete.filtered = filteredList.filter(x =>
        x.startsWith(command)
      );
      this.autocomplete.command = command;
    }
  }
};
</script>

<style lang="less" scoped>
.debug-console {
  width: 100%;
  height: 100%;
}

.output-buttons {
  line-height: 20px;
  text-align: right;

  & .button {
    margin: 4px;
    cursor: pointer;
  }
}

.output {
  flex: 1;
  margin-bottom: 8px;
  border: 1px solid #ccc;
  user-select: text;
  padding: 4px;

  & .help {
    color: var(--primary-color);
  }

  & * {
    user-select: text;
  }

  & .error {
    color: var(--error-color);
  }
}

.row {
  display: flex;
  flex-direction: row;
  padding-top: 5px;

  & > * {
    padding-right: 5px;
  }

  & > .icon {
    flex: 0 0 20px;
  }
}

.row:last-of-type {
  margin-bottom: 10px;
}

pre {
  white-space: pre-wrap;
}
</style>

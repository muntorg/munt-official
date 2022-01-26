<template>
  <div class="debug-console flex-col">
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
    <div
      ref="output"
      class="output scrollable scrollable-x"
      :style="outputStyle"
    >
      <div class="info">
        Use up and down arrows to navigate history. Type
        <span class="help">help</span> for an overview of available commands.
      </div>

      <div v-for="(item, index) in output" :key="index">
        <div v-if="item.type === 'command'">
          <fa-icon class="icon" :icon="['fal', 'angle-double-left']" />
          {{ item.data }}
        </div>

        <pre v-else :style="outputStyle"
          >{{ item.data }}
          </pre
        >
      </div>
    </div>
    <div class="input">
      <input
        ref="command"
        type="text"
        spellcheck="false"
        v-model="command"
        @keydown="onRpcInputKeyDown"
        list="commands"
      />
      <datalist id="commands">
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

var updateFilterTimeout = null;

export default {
  name: "DebugConsole",
  data() {
    return {
      command: "",
      fontSize: 0.8,
      autocompleteList: [],
      filteredAutocompleteList: [],
      preventAutocompleteList: false
    };
  },
  props: {
    value: {
      type: Object,
      default: () => {
        return {
          output: [],
          commands: [],
          idx: 0
        };
      }
    }
  },
  computed: {
    output() {
      return this.value.output;
    },
    commands() {
      return this.value.commands;
    },
    idx() {
      return this.value.idx;
    },
    outputStyle() {
      return `font-size: ${this.fontSize}rem;`;
    }
  },
  created() {
    this.autocompleteList = RpcController.GetAutocompleteList();
  },
  mounted() {
    this.scrollToBottom();
    this.focusCommand();
  },
  watch: {
    command() {
      this.filterAutocompleteList();
    }
  },
  methods: {
    formatData(input) {
      let split = input.split("\n");
      let output = "";
      for (var i = 0; i < split.length; i++) {
        output += `${split[i]}<br />`;
      }
      return output;
    },
    moveToEnd() {
      let command = this.$refs.command;
      setTimeout(function() {
        command.selectionStart = command.selectionEnd = command.value.length;
      }, 0);
    },
    async onRpcInputKeyDown() {
      this.preventAutocompleteList = false;
      switch (event.keyCode) {
        case 13: {
          let result = await RpcController.Execute(this.command);
          this.command = "";

          this.value.output.push({
            type: "command",
            data: result.command
          });
          this.value.output.push({ type: "result", ...result });

          this.scrollToBottom();

          let index = this.commands.indexOf(result.command);
          if (index !== -1) {
            this.value.commands.splice(index, 1);
          }
          this.value.commands.push(result.command);
          this.value.idx = this.commands.length;
          return;
        }
        case 38: // arrow up
          if (this.idx > 0) this.value.idx--;
          if (this.commands.length > 0) {
            this.preventAutocompleteList = true;
            this.command = this.commands[this.idx];
            this.moveToEnd();
            event.preventDefault();
          }
          break;
        case 40: // arrow down
          if (this.idx < this.commands.length) this.value.idx++;
          if (this.idx < this.commands.length && this.commands.length > 0) {
            this.preventAutocompleteList = true;
            this.command = this.commands[this.idx];
            this.moveToEnd();
            event.preventDefault();
          } else {
            this.command = "";
          }
          break;
      }
    },
    clearOutput() {
      this.$emit("clear-output");
      this.focusCommand();
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
    filterAutocompleteList(delay = true) {
      clearTimeout(updateFilterTimeout);

      if (this.preventAutocompleteList) return;
      if (delay) {
        updateFilterTimeout = setTimeout(() => {
          this.filterAutocompleteList(false);
        }, 500);
        return;
      }

      let filteredAutocompleteList = [];

      if (this.command && this.command.length > 0) {
        filteredAutocompleteList = this.autocompleteList.filter(item => {
          return item.startsWith(this.command);
        });
        if (filteredAutocompleteList.length > 20) filteredAutocompleteList = [];
      }

      this.filteredAutocompleteList = filteredAutocompleteList;
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

  & > .info {
    padding: 8px 0;
  }

  & .help {
    color: var(--primary-color);
  }

  & * {
    user-select: text;
  }

  //font-size: 0.8rem;

  & .error {
    color: var(--error-color);
  }
}

.input > input::-webkit-calendar-picker-indicator {
  display: none;
}

pre {
  overflow-wrap: break-word;
}
</style>

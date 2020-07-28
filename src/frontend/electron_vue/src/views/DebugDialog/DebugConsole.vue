<template>
  <div class="debug-console flex-col">
    <div ref="output" class="output">
      <div v-for="(item, index) in output" :key="index">
        <div v-if="item.type === 'command'">
          <fa-icon class="icon" :icon="['fal', 'angle-double-left']" />
          {{ item.data }}
        </div>

        <pre v-else
          >{{ item.data }}
          </pre
        >
      </div>
    </div>
    <div class="input">
      <input
        ref="command"
        type="text"
        v-model="command"
        @keydown="onRpcInputKeyDown"
      />
    </div>
  </div>
</template>

<script>
import UnityBackend from "../../unity/UnityBackend";

export default {
  name: "DebugConsole",
  data() {
    return {
      command: "",
      commands: [],
      output: [],
      idx: 0
    };
  },
  mounted() {
    this.$nextTick(() => {
      this.$refs.command.focus();
    });
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
    onRpcInputKeyDown() {
      switch (event.keyCode) {
        case 13: {
          this.output.push({ type: "command", data: this.command });

          let result = UnityBackend.ExecuteRpc(this.command);

          this.output.push({ type: "result", ...result });

          this.$nextTick(() => {
            this.$refs.output.scrollTop = this.$refs.output.scrollHeight;
          });

          let idx = this.commands.indexOf(this.command);
          if (idx !== -1) {
            this.commands.splice(idx, 1);
          }
          this.commands.push(this.command);
          this.command = "";
          this.idx = this.commands.length;
          break;
        }
        case 38: // arrow up
          if (this.idx > 0) this.idx--;
          if (this.commands.length > 0) {
            this.command = this.commands[this.idx];
          }
          break;
        case 40: // arrow down
          if (this.idx < this.commands.length) this.idx++;
          if (this.idx < this.commands.length && this.commands.length > 0) {
            this.command = this.commands[this.idx];
          } else {
            this.command = "";
          }
          break;
      }
    }
  }
};
</script>

<style lang="less" scoped>
.debug-console {
  width: 100%;
  height: 100%;
}

.output {
  flex: 1;
  margin-bottom: 8px;
  border: 1px solid #ccc;
  overflow-y: auto;
  user-select: text;

  & * {
    user-select: text;
  }

  font-size: 0.8rem;

  & .icon {
    margin: 0 8px;
  }

  & .error {
    color: var(--error-color);
  }
}

pre {
  overflow-wrap: break-word;
  font-size: 0.8rem;
}
</style>

<template>
  <div class="app-debug">
    <modal-dialog v-model="modal" />
    <div class="main scrollable">
      Copyright (C) 2023 "The Centure developers"<br /><br />
      This program is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free
      Software Foundation; either version 2.1 of the License, or (at your option) any later version.<br /><br />
      This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
      FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.<br /><br />
      Portions of this software not written by us, and libraries that we make use of are licensed under a variety of other licenses for more licensing
      information and a copy of the source code visit <a href="https://github.com/muntorg/munt-official/blob/master/COPYING">our code repository</a
      ><br /><br />
      The below is a list of most major libraries used and their licensing, the list may not be comprehensive for a comprehensive list consult the
      repository.<br />
      <div class="copyright-table-wrapper">
        <table class="copyright-table">
          <tr>
            <td>Bitcoin</td>
            <td>The MIT License</td>
          </tr>
          <tr>
            <td>Boost</td>
            <td>Boost Software License 1.0</td>
          </tr>
          <tr>
            <td>BDB</td>
            <td>The MIT license</td>
          </tr>
          <tr>
            <td>Cryptopp</td>
            <td>Boost Software License 1.0</td>
          </tr>
          <tr>
            <td>Electron</td>
            <td>The MIT License</td>
          </tr>
          <tr>
            <td>Nodejs</td>
            <td>The MIT License</td>
          </tr>
          <tr>
            <td>OpenSSL</td>
            <td>Apache License v2</td>
          </tr>
          <tr>
            <td>Protobuf</td>
            <td>BSD</td>
          </tr>
          <tr>
            <td>ZeroMQ</td>
            <td>Mozilla Public License 2.0</td>
          </tr>
          <tr>
            <td>Zlib</td>
            <td>Zlib</td>
          </tr>
        </table>
      </div>
    </div>
  </div>
</template>

<script>
import ModalDialog from "../../components/ModalDialog";
import EventBus from "../../EventBus.js";

export default {
  data() {
    return {
      modal: null
    };
  },
  components: {
    ModalDialog
  },
  mounted() {
    EventBus.$on("close-dialog", this.closeModal);
    EventBus.$on("show-dialog", this.showModal);
  },
  beforeDestroy() {
    EventBus.$off("close-dialog", this.closeModal);
    EventBus.$off("show-dialog", this.showModal);
  },
  methods: {
    closeModal() {
      this.modal = null;
    },
    showModal(modal) {
      this.modal = modal;
    }
  }
};
</script>

<style lang="less" scoped>
.app-about {
  width: 100%;
  height: 100vh;
  overflow: hidden;
  font-size: 0.85em;
}

.main {
  height: calc(100% - 48px);
  padding: 20px;
  overflow-x: hidden;
  overflow-y: auto;
}

.copyright-table-wrapper {
  display: block;
  width: 100%;
  overflow: hidden;
  font-size: 0.85em;
}

.copyright-table {
  margin-top: 25px;
  margin-left: auto;
  margin-right: auto;
  font-size: 0.9em;
  font-family: sans-serif;
  border: 1px solid black;
  border-collapse: collapse;
}
.copyright-table tr {
  border: 1px solid black;
  border-collapse: collapse;
}
.copyright-table td {
  padding: 5px;
  padding-left: 15px;
  padding-right: 15px;
  border: 1px solid black;
  border-collapse: collapse;
}
</style>

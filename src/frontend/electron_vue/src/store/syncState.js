import { ipcMain, ipcRenderer } from "electron";
import cloneDeep from "lodash.clonedeep";

export default store => {
  if (process.type === "renderer") {
    // replace the renderer state by the main state
    ipcRenderer.on("vuex-connected", (event, state) => {
      console.log("vuex-connected");
      store.replaceState(cloneDeep(state));
    });
    // send vuex-connect message to receive the main state
    ipcRenderer.send("vuex-connect");
  } else {
    // on vuex-connect send the current state to the renderer
    ipcMain.on("vuex-connect", event => {
      console.log("vuex-connect");
      console.log(
        `coreReady: ${store.state.coreReady} status: ${store.state.status}`
      );
      event.sender.send("vuex-connected", store.state);
    });
  }
};

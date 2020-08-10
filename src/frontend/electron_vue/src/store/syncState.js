import { ipcMain, ipcRenderer } from "electron";
import cloneDeep from "lodash.clonedeep";

export default store => {
  if (process.type === "renderer") {
    // replace the renderer state by the main state
    ipcRenderer.on("vuex-connected", (event, state) => {
      console.log("vuex-connected");
      let newState = cloneDeep(state);
      store.replaceState(newState);
    });
    // send vuex-connect message to receive the main state
    ipcRenderer.send("vuex-connect");
  } else {
    // on vuex-connect send the current state to the renderer
    ipcMain.on("vuex-connect", event => {
      console.log("vuex-connect");
      event.sender.send("vuex-connected", store.state);
    });
  }
};

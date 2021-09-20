Module.loadFile = (function () {
  var loadGame = cwrap("loadGame", "number", ["string"]);
  return function (name) {
    if (loadGame(name)) {
      var arr = name.split(".");
      arr.pop();
      Module.gameName = name;
      Module.saveName = arr.join(".") + ".sav";
      return true;
    }
    return false;
  };
})();

Module.mute = () => ccall("mute", "null", []);

Module.unMute = () => ccall("unMute", "null", []);

Module.setSpeed = cwrap("setSpeed", "null", ["numbers"]);
Module.setScale = cwrap("setScale", "null", ["numbers"]);
Module.getPlatform = cwrap("getPlatform", "string", []);

//getters and setters for every key available to gba
Module.setKeyA = cwrap("setKeyA", "null", ["string"]);
Module.getKeyA = cwrap("getKeyA", "string", []);

Module.setKeyB = cwrap("setKeyB", "null", ["string"]);
Module.getKeyB = cwrap("getKeyB", "string", []);

Module.setKeyL = cwrap("setKeyL", "null", ["string"]);
Module.getKeyL = cwrap("getKeyL", "string", []);

Module.setKeyR = cwrap("setKeyR", "null", ["string"]);
Module.getKeyR = cwrap("getKeyR", "string", []);

Module.setKeyStart = cwrap("setKeyStart", "null", ["string"]);
Module.getKeyStart = cwrap("getKeyStart", "string", []);

Module.setKeySelect = cwrap("setKeySelect", "null", ["string"]);
Module.getKeySelect = cwrap("getKeySelect", "string", []);

Module.setKeyUp = cwrap("setKeyUp", "null", ["string"]);
Module.getKeyUp = cwrap("getKeyUp", "string", []);

Module.setKeyDown = cwrap("setKeyDown", "null", ["string"]);
Module.getKeyDown = cwrap("getKeyDown", "string", []);

Module.setKeyLeft = cwrap("setKeyLeft", "null", ["string"]);
Module.getKeyLeft = cwrap("getKeyLeft", "string", []);

Module.setKeyRight = cwrap("setKeyRight", "null", ["string"]);
Module.getKeyRight = cwrap("getKeyRight", "string", []);

Module.getSave = function () {
  return FS.readFile("/data/saves/" + Module.saveName);
};

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

Module.getSave = function () {
  return FS.readFile("/data/saves/" + Module.saveName);
};

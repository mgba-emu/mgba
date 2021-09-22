function findFunctions(mod) {
  let keys = [
    "A",
    "B",
    "L",
    "R",
    "Start",
    "Select",
    "Up",
    "Down",
    "Left",
    "Right",
    "Forward",
  ];
  keys.forEach((key) => {
    mod[`getKey${key}`] = cwrap(`getKey${key}`, "string", []);
    mod[`setKey${key}`] = cwrap(`setKey${key}`, "null", ["string"]);
  });
}

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

findFunctions(Module);

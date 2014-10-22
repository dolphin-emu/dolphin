/* gametdb-to-gameini.js
 *    nodejs script that fetches http://www.gametdb.com/titles.txt
 *    and updates Dolphin's GameINI files from it.
 * by @waddlesplash, 10/22/14. GPLv2+.
 */

var fs = require('fs');
var gameTxtData = '';
var gameTxt = {};

function indexOfTitleEquals(game) {
  var temp = game.indexOf("[EmuState]");
  if (temp == -1)
    return -1;
  return game.indexOf("Title = ", temp);
}

function indexOfFirstEmuStateLine(game) {
  var temp = game.indexOf("[EmuState]");
  if (temp == -1)
    return -1;
  return game.indexOf("\n", temp) + 1;
}

function doWork() {
  console.log("Parsing titles.txt...");

  var rows = gameTxtData.replace("\r\n", "\n")
                        .replace("\r", "\n")
                        .split("\n");
  for (var i in rows) {
    var items = rows[i].split(" = ");
    gameTxt[items[0].toUpperCase()] = items[1];
  }

  console.log("Updating GameINI files...");

  var gamesNotInDb = 0;
  var dir = fs.readdirSync("../Data/Sys/GameSettings/");
  for (var i in dir) {
    var gameID = dir[i].substring(0, dir[i].length - 4).toUpperCase();
    if (gameTxt[gameID] != undefined) {
      // Update GameINI file
      var game = fs.readFileSync("../Data/Sys/GameSettings/" + dir[i], {'encoding': 'utf8'});
      var winEOL = game.indexOf("\r\n") != -1;
      var eqIndex = indexOfTitleEquals(game);
      if (eqIndex > 0) {
        game = game.substring(0, eqIndex)
              + ' ' + gameTxt[gameID]
              + game.substring(game.indexOf("\n", eqIndex), game.length);
      } else {
        var shIndex = indexOfFirstEmuStateLine(game);
        game = game.substring(0, shIndex)
               + "Title = " + gameTxt[gameID] + (winEOL ? "\r\n" : "\n")
               + game.substring(shIndex, game.length);
      }
      fs.writeFileSync("../Data/Sys/GameSettings/" + dir[i], game);
    } else
      gamesNotInDb++;
  }

  console.log("Done! " + gamesNotInDb + " games not found in database.");
}

var callback = function(response) {
  response.on('data', function (chunk) {
    gameTxtData += chunk;
  });
  response.on('end', doWork);
};

console.log("Fetching titles.txt...");
require('http').request("http://www.gametdb.com/titles.txt", callback).end();

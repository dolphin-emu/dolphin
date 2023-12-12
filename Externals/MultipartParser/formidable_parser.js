var Buffer = require('buffer').Buffer
  , s = 0
  , S =
    { PARSER_UNINITIALIZED: s++
    , START: s++
    , START_BOUNDARY: s++
    , HEADER_FIELD_START: s++
    , HEADER_FIELD: s++
    , HEADER_VALUE_START: s++
    , HEADER_VALUE: s++
    , HEADER_VALUE_ALMOST_DONE: s++
    , HEADERS_ALMOST_DONE: s++
    , PART_DATA_START: s++
    , PART_DATA: s++
    , PART_END: s++
    , END: s++
    }

  , f = 1
  , F =
    { PART_BOUNDARY: f
    , LAST_BOUNDARY: f *= 2
    }

  , LF = 10
  , CR = 13
  , SPACE = 32
  , HYPHEN = 45
  , COLON = 58
  , A = 97
  , Z = 122

  , lower = function(c) {
      return c | 0x20;
    };

for (var s in S) {
  exports[s] = S[s];
}

function MultipartParser() {
  this.boundary = null;
  this.boundaryChars = null;
  this.lookbehind = null;
  this.state = S.PARSER_UNINITIALIZED;

  this.index = null;
  this.flags = 0;
};
exports.MultipartParser = MultipartParser;

MultipartParser.prototype.initWithBoundary = function(str) {
  this.boundary = new Buffer(str.length+4);
  this.boundary.write('\r\n--', 'ascii', 0);
  this.boundary.write(str, 'ascii', 4);
  this.lookbehind = new Buffer(this.boundary.length+8);
  this.state = S.START;

  this.boundaryChars = {};
  for (var i = 0; i < this.boundary.length; i++) {
    this.boundaryChars[this.boundary[i]] = true;
  }
};

MultipartParser.prototype.write = function(buffer) {
  var self = this
    , i = 0
    , len = buffer.length
    , prevIndex = this.index
    , index = this.index
    , state = this.state
    , flags = this.flags
    , lookbehind = this.lookbehind
    , boundary = this.boundary
    , boundaryChars = this.boundaryChars
    , boundaryLength = this.boundary.length
    , boundaryEnd = boundaryLength - 1
    , bufferLength = buffer.length
    , c
    , cl

    , mark = function(name) {
        self[name+'Mark'] = i;
      }
    , clear = function(name) {
        delete self[name+'Mark'];
      }
    , callback = function(name, buffer, start, end) {
        if (start !== undefined && start === end) {
          return;
        }

        var callbackSymbol = 'on'+name.substr(0, 1).toUpperCase()+name.substr(1);
        if (callbackSymbol in self) {
          self[callbackSymbol](buffer, start, end);
        }
      }
    , dataCallback = function(name, clear) {
        var markSymbol = name+'Mark';
        if (!(markSymbol in self)) {
          return;
        }

        if (!clear) {
          callback(name, buffer, self[markSymbol], buffer.length);
          self[markSymbol] = 0;
        } else {
          callback(name, buffer, self[markSymbol], i);
          delete self[markSymbol];
        }
      };

  for (i = 0; i < len; i++) {
    c = buffer[i];
    switch (state) {
      case S.PARSER_UNINITIALIZED:
        return i;
      case S.START:
        index = 0;
        state = S.START_BOUNDARY;
      case S.START_BOUNDARY:
        if (index == boundary.length - 2) {
          if (c != CR) {
            return i;
          }
          index++;
          break;
        } else if (index - 1 == boundary.length - 2) {
          if (c != LF) {
            return i;
          }
          index = 0;
          callback('partBegin');
          state = S.HEADER_FIELD_START;
          break;
        }

        if (c != boundary[index+2]) {
          return i;
        }
        index++;
        break;
      case S.HEADER_FIELD_START:
        state = S.HEADER_FIELD;
        mark('headerField');
      case S.HEADER_FIELD:
        if (c == CR) {
          clear('headerField');
          state = S.HEADERS_ALMOST_DONE;
          break;
        }

        if (c == HYPHEN) {
          break;
        }

        if (c == COLON) {
          dataCallback('headerField', true);
          state = S.HEADER_VALUE_START;
          break;
        }

        cl = lower(c);
        if (cl < A || cl > Z) {
          return i;
        }
        break;
      case S.HEADER_VALUE_START:
        if (c == SPACE) {
          break;
        }

        mark('headerValue');
        state = S.HEADER_VALUE;
      case S.HEADER_VALUE:
        if (c == CR) {
          dataCallback('headerValue', true);
          state = S.HEADER_VALUE_ALMOST_DONE;
        }
        break;
      case S.HEADER_VALUE_ALMOST_DONE:
        if (c != LF) {
          return i;
        }
        state = S.HEADER_FIELD_START;
        break;
      case S.HEADERS_ALMOST_DONE:
        if (c != LF) {
          return i;
        }

        state = S.PART_DATA_START;
        break;
      case S.PART_DATA_START:
        state = S.PART_DATA
        mark('partData');
      case S.PART_DATA:
        prevIndex = index;

        if (index == 0) {
          // boyer-moore derrived algorithm to safely skip non-boundary data
          while (i + boundaryLength <= bufferLength) {
            if (buffer[i + boundaryEnd] in boundaryChars) {
              break;
            }

            i += boundaryLength;
          }
          c = buffer[i];
        }

        if (index < boundary.length) {
          if (boundary[index] == c) {
            if (index == 0) {
              dataCallback('partData', true);
            }
            index++;
          } else {
            index = 0;
          }
        } else if (index == boundary.length) {
          index++;
          if (c == CR) {
            // CR = part boundary
            flags |= F.PART_BOUNDARY;
          } else if (c == HYPHEN) {
            // HYPHEN = end boundary
            flags |= F.LAST_BOUNDARY;
          } else {
            index = 0;
          }
        } else if (index - 1 == boundary.length)  {
          if (flags & F.PART_BOUNDARY) {
            index = 0;
            if (c == LF) {
              // unset the PART_BOUNDARY flag
              flags &= ~F.PART_BOUNDARY;
              callback('partEnd');
              callback('partBegin');
              state = S.HEADER_FIELD_START;
              break;
            }
          } else if (flags & F.LAST_BOUNDARY) {
            if (c == HYPHEN) {
              index++;
            } else {
              index = 0;
            }
          } else {
            index = 0;
          }
        } else if (index - 2 == boundary.length)  {
          if (c == CR) {
            index++;
          } else {
            index = 0;
          }
        } else if (index - boundary.length == 3)  {
          index = 0;
          if (c == LF) {
            callback('partEnd');
            callback('end');
            state = S.END;
            break;
          }
        }

        if (index > 0) {
          // when matching a possible boundary, keep a lookbehind reference
          // in case it turns out to be a false lead
          lookbehind[index-1] = c;
        } else if (prevIndex > 0) {
          // if our boundary turned out to be rubbish, the captured lookbehind
          // belongs to partData
          callback('partData', lookbehind, 0, prevIndex);
          prevIndex = 0;
          mark('partData');
        }
        
        break;
      default:
        return i;
    }
  }

  dataCallback('headerField');
  dataCallback('headerValue');
  dataCallback('partData');

  this.index = index;
  this.state = state;
  this.flags = flags;

  return len;
};

MultipartParser.prototype.end = function() {
  if (this.state != S.END) {
    return new Error('MultipartParser.end(): stream ended unexpectedly');
  }
};

function sprintf ( ) {
    // http://kevin.vanzonneveld.net
    // +   original by: Ash Searle (http://hexmen.com/blog/)
    // + namespaced by: Michael White (http://getsprink.com)
    // +    tweaked by: Jack
    // +   improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
    // +      input by: Paulo Freitas
    // +   improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
    // +      input by: Brett Zamir (http://brett-zamir.me)
    // +   improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
    // *     example 1: sprintf("%01.2f", 123.1);
    // *     returns 1: 123.10
    // *     example 2: sprintf("[%10s]", 'monkey');
    // *     returns 2: '[    monkey]'
    // *     example 3: sprintf("[%'#10s]", 'monkey');
    // *     returns 3: '[####monkey]'

    var regex = /%%|%(\d+\$)?([-+\'#0 ]*)(\*\d+\$|\*|\d+)?(\.(\*\d+\$|\*|\d+))?([scboxXuidfegEG])/g;
    var a = arguments, i = 0, format = a[i++];

    // pad()
    var pad = function (str, len, chr, leftJustify) {
        if (!chr) {chr = ' ';}
        var padding = (str.length >= len) ? '' : Array(1 + len - str.length >>> 0).join(chr);
        return leftJustify ? str + padding : padding + str;
    };

    // justify()
    var justify = function (value, prefix, leftJustify, minWidth, zeroPad, customPadChar) {
        var diff = minWidth - value.length;
        if (diff > 0) {
            if (leftJustify || !zeroPad) {
                value = pad(value, minWidth, customPadChar, leftJustify);
            } else {
                value = value.slice(0, prefix.length) + pad('', diff, '0', true) + value.slice(prefix.length);
            }
        }
        return value;
    };

    // formatBaseX()
    var formatBaseX = function (value, base, prefix, leftJustify, minWidth, precision, zeroPad) {
        // Note: casts negative numbers to positive ones
        var number = value >>> 0;
        prefix = prefix && number && {'2': '0b', '8': '0', '16': '0x'}[base] || '';
        value = prefix + pad(number.toString(base), precision || 0, '0', false);
        return justify(value, prefix, leftJustify, minWidth, zeroPad);
    };

    // formatString()
    var formatString = function (value, leftJustify, minWidth, precision, zeroPad, customPadChar) {
        if (precision != null) {
            value = value.slice(0, precision);
        }
        return justify(value, '', leftJustify, minWidth, zeroPad, customPadChar);
    };

    // doFormat()
    var doFormat = function (substring, valueIndex, flags, minWidth, _, precision, type) {
        var number;
        var prefix;
        var method;
        var textTransform;
        var value;

        if (substring == '%%') {return '%';}

        // parse flags
        var leftJustify = false, positivePrefix = '', zeroPad = false, prefixBaseX = false, customPadChar = ' ';
        var flagsl = flags.length;
        for (var j = 0; flags && j < flagsl; j++) {
            switch (flags.charAt(j)) {
                case ' ': positivePrefix = ' '; break;
                case '+': positivePrefix = '+'; break;
                case '-': leftJustify = true; break;
                case "'": customPadChar = flags.charAt(j+1); break;
                case '0': zeroPad = true; break;
                case '#': prefixBaseX = true; break;
            }
        }

        // parameters may be null, undefined, empty-string or real valued
        // we want to ignore null, undefined and empty-string values
        if (!minWidth) {
            minWidth = 0;
        } else if (minWidth == '*') {
            minWidth = +a[i++];
        } else if (minWidth.charAt(0) == '*') {
            minWidth = +a[minWidth.slice(1, -1)];
        } else {
            minWidth = +minWidth;
        }

        // Note: undocumented perl feature:
        if (minWidth < 0) {
            minWidth = -minWidth;
            leftJustify = true;
        }

        if (!isFinite(minWidth)) {
            throw new Error('sprintf: (minimum-)width must be finite');
        }

        if (!precision) {
            precision = 'fFeE'.indexOf(type) > -1 ? 6 : (type == 'd') ? 0 : undefined;
        } else if (precision == '*') {
            precision = +a[i++];
        } else if (precision.charAt(0) == '*') {
            precision = +a[precision.slice(1, -1)];
        } else {
            precision = +precision;
        }

        // grab value using valueIndex if required?
        value = valueIndex ? a[valueIndex.slice(0, -1)] : a[i++];

        switch (type) {
            case 's': return formatString(String(value), leftJustify, minWidth, precision, zeroPad, customPadChar);
            case 'c': return formatString(String.fromCharCode(+value), leftJustify, minWidth, precision, zeroPad);
            case 'b': return formatBaseX(value, 2, prefixBaseX, leftJustify, minWidth, precision, zeroPad);
            case 'o': return formatBaseX(value, 8, prefixBaseX, leftJustify, minWidth, precision, zeroPad);
            case 'x': return formatBaseX(value, 16, prefixBaseX, leftJustify, minWidth, precision, zeroPad);
            case 'X': return formatBaseX(value, 16, prefixBaseX, leftJustify, minWidth, precision, zeroPad).toUpperCase();
            case 'u': return formatBaseX(value, 10, prefixBaseX, leftJustify, minWidth, precision, zeroPad);
            case 'i':
            case 'd':
                number = parseInt(+value, 10);
                prefix = number < 0 ? '-' : positivePrefix;
                value = prefix + pad(String(Math.abs(number)), precision, '0', false);
                return justify(value, prefix, leftJustify, minWidth, zeroPad);
            case 'e':
            case 'E':
            case 'f':
            case 'F':
            case 'g':
            case 'G':
                number = +value;
                prefix = number < 0 ? '-' : positivePrefix;
                method = ['toExponential', 'toFixed', 'toPrecision']['efg'.indexOf(type.toLowerCase())];
                textTransform = ['toString', 'toUpperCase']['eEfFgG'.indexOf(type) % 2];
                value = prefix + Math.abs(number)[method](precision);
                return justify(value, prefix, leftJustify, minWidth, zeroPad)[textTransform]();
            default: return substring;
        }
    };

    return format.replace(regex, doFormat);
}



var fs = require('fs');
var sys = require('sys');


var FILENAME = 'input3.txt';
//var BOUNDARY = 'abcd';
var BOUNDARY = "-----------------------------168072824752491622650073";
var TIMES = 10;
var SLURP = true;
var QUIET = true;


function createParser() {
  var parser = new MultipartParser();
  parser.initWithBoundary(BOUNDARY);
  if (!QUIET) {
    parser.onPartBegin = function() {
      sys.puts('onPartBegin');
    };
    parser.onHeaderField = function(buffer, start, end) {
      sys.puts('onHeaderField: ' + buffer.slice(start, end));
    };
    parser.onHeaderValue = function(buffer, start, end) {
      sys.puts('onHeaderValue: ' + buffer.slice(start, end));
    };
    parser.onPartData = function(buffer, start, end) {
      //sys.puts('onPartData: ' + buffer.slice(start, end));
      //sys.puts('onPartData: ' + (end - start) + ' bytes');
    };
    parser.onPartEnd = function() {
      sys.puts('onPartEnd');
    };
    parser.onEnd = function() {
      sys.puts('onEnd');
    };
  }
  return parser;
}

var parser, i, eof, bytesRead, buf, error, stime, etime;
var stats = fs.statSync(FILENAME);
if (SLURP) {
  fd = fs.openSync(FILENAME, 'r', 0);
  buf = new Buffer(stats.size, 'binary');
  fs.readSync(fd, buf, 0, buf.length, null);
  
  stime = new Date();
  for (i = 0; i < TIMES; i++) {
    parser = createParser();
    parser.write(buf);
  }
} else {
  buf = new Buffer(1024 * 32, 'binary');
  stime = new Date();
  for (i = 0; i < TIMES; i++) {
    parser = createParser();
    if (!QUIET) {
      sys.puts('-------------------');
    }
    fd = fs.openSync(FILENAME, 'r', 0);
    eof = false;
    totalRead = 0;
    while (!eof) {
      bytesRead = fs.readSync(fd, buf, 0, buf.length, null);
      eof = bytesRead == 0;
      if (!eof) {
        parser.write(buf);
      }
    }
    parser.end();
  }
}

etime = new Date();
var diff = etime.getTime() - stime.getTime();
sys.puts(sprintf("(JS)     Total: %.2fs   Per run: %.2fs   Throughput: %.2f MB/sec",
  diff / 1000.0,
  diff / TIMES / 1000.0,
  (stats.size * TIMES) / (diff / 1000) / 1024.0 / 1024.0
));
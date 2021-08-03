function decodeUplink(input) {
  var data = {};
  data.latitude = ((input.bytes[0]<<16)>>>0) + ((input.bytes[1]<<8)>>>0) + input.bytes[2];
  data.latitude = (data.latitude / 16777215.0 * 180) - 90;
  data.longitude = ((input.bytes[3]<<16)>>>0) + ((input.bytes[4]<<8)>>>0) + input.bytes[5];
  data.longitude = (data.longitude / 16777215.0 * 360) - 180;
  var altValue = ((input.bytes[6]<<8)>>>0) + input.bytes[7];
  var sign = input.bytes[6] & (1 << 7);
  if(sign) {
    data.altitude = 0xFFFF0000 | altValue;
  }
  else {
    data.altitude = altValue;
  }
  data.hdop = input.bytes[8] / 10.0;
  data.option = input.bytes[9];
  return {
    data: data
  };
}


--- old (V2) ---
function Decoder(bytes, port) {
  // Decode an uplink message from a buffer of bytes to an object of fields.
  var decoded = {};
  decoded.latitude = ((bytes[0]<<16)>>>0) + ((bytes[1]<<8)>>>0) + bytes[2];
  decoded.latitude = (decoded.latitude / 16777215.0 * 180) - 90;
  decoded.latitude = decoded.latitude.toFixed(5);
  decoded.longitude = ((bytes[3]<<16)>>>0) + ((bytes[4]<<8)>>>0) + bytes[5];
  decoded.longitude = (decoded.longitude / 16777215.0 * 360) - 180;
  decoded.longitude = decoded.longitude.toFixed(5);
  var altValue = ((bytes[6]<<8)>>>0) + bytes[7];
  var sign = bytes[6] & (1 << 7);
  if(sign) {
    decoded.altitude = 0xFFFF0000 | altValue;
  }
  else {
    decoded.altitude = altValue;
  }
  decoded.hdop = bytes[8] / 10.0;
  decoded.hdop = decoded.hdop.toFixed(1);
  decoded.option = bytes[9];
  return decoded;
}
